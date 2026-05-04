#include "wifi_ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>

#ifndef DISABLE_GITHUB_OTA
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#endif

#include "rgb_panel.h"
#include "wifi_credentials.h"

// Build-time identity (overridden by CI: -DFIRMWARE_VERSION=\"<sha>\").
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
// GitHub repo "owner/name" hosting the firmware-latest release.
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif

#define OTA_RELEASE_TAG  "firmware-latest"
#define OTA_VERSION_URL  "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/version.txt"
#define OTA_FIRMWARE_URL "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/firmware.bin"

namespace wifi_ota {

volatile bool inProgress = false;

void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("WiFi: connecting to '%s'", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(200);
    if (millis() - start > 30000) {
      Serial.println("\nWiFi: timeout, restarting...");
      ESP.restart();
    }
  }
  Serial.printf("\nWiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
}

void setupArduinoOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA
    .onStart([]() {
      inProgress = true;
      Serial.printf("OTA: start (%s)\n",
                    ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "fs");
      rgb_panel::blank();
      rgb_panel::showOtaProgress(0);
    })
    .onEnd([]() {
      Serial.println("\nOTA: done");
      rgb_panel::showOtaProgress(100);
      inProgress = false;
    })
    .onProgress([](unsigned int p, unsigned int t) {
      if (t == 0) return;
      int pct = (int)((uint64_t)p * 100 / t);
      static int lastPct = -1;
      if (pct != lastPct) {
        lastPct = pct;
        Serial.printf("OTA: %d%%\r", pct);
        rgb_panel::showOtaProgress(pct);
      }
    })
    .onError([](ota_error_t e) {
      inProgress = false;
      Serial.printf("OTA error[%u]: ", e);
      switch (e) {
        case OTA_AUTH_ERROR:    Serial.println("auth");    break;
        case OTA_BEGIN_ERROR:   Serial.println("begin");   break;
        case OTA_CONNECT_ERROR: Serial.println("connect"); break;
        case OTA_RECEIVE_ERROR: Serial.println("receive"); break;
        case OTA_END_ERROR:     Serial.println("end");     break;
      }
    });

  ArduinoOTA.begin();
  Serial.printf("OTA: ready at %s.local (%s)\n",
                OTA_HOSTNAME, WiFi.localIP().toString().c_str());
}

void handleOta() {
  ArduinoOTA.handle();
}

#ifndef DISABLE_GITHUB_OTA
namespace {

String fetchLatestVersion() {
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("f1-esp32-ota");
  if (!http.begin(client, OTA_VERSION_URL)) {
    Serial.println("OTA-check: http.begin failed");
    return String();
  }
  int code = http.GET();
  String body;
  if (code == HTTP_CODE_OK) {
    body = http.getString();
    body.trim();
  } else {
    Serial.printf("OTA-check: HTTP %d\n", code);
  }
  http.end();
  return body;
}

}  // namespace

void checkAndUpdateFromGithub() {
  Serial.printf("OTA-check: running version '%s'\n", FIRMWARE_VERSION);
  String latest = fetchLatestVersion();
  if (latest.isEmpty()) {
    Serial.println("OTA-check: could not read latest version, skipping");
    return;
  }
  Serial.printf("OTA-check: latest = '%s'\n", latest.c_str());
  if (latest == FIRMWARE_VERSION) {
    Serial.println("OTA-check: already up to date");
    return;
  }

  Serial.println("OTA-check: new version available, downloading...");
  rgb_panel::blank();
  rgb_panel::showOtaProgress(0);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);

  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onStart([]() {
    Serial.println("HTTPUpdate: start");
    rgb_panel::showOtaProgress(0);
  });
  httpUpdate.onProgress([](int cur, int total) {
    if (total <= 0) return;
    int pct = (int)((int64_t)cur * 100 / total);
    static int lastPct = -1;
    if (pct != lastPct) {
      lastPct = pct;
      rgb_panel::showOtaProgress(pct);
      if (pct % 10 == 0) Serial.printf("HTTPUpdate: %d%%\n", pct);
    }
  });
  httpUpdate.onError([](int err) {
    Serial.printf("HTTPUpdate: error %d: %s\n",
                  err, httpUpdate.getLastErrorString().c_str());
  });

  t_httpUpdate_return res = httpUpdate.update(client, OTA_FIRMWARE_URL, FIRMWARE_VERSION);
  switch (res) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTPUpdate: failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTPUpdate: no updates");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTPUpdate: ok (rebooting)");
      break;
  }
}
#else  // DISABLE_GITHUB_OTA
void checkAndUpdateFromGithub() {
  Serial.println("OTA-check: GitHub self-update disabled at build time");
}
#endif

}  // namespace wifi_ota

