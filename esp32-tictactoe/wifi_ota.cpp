#include "wifi_ota.h"

#include <Arduino.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>
#include <WiFi.h>
#include <WiFiManager.h>

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
// Asset name on the rolling release. The tic-tac-toe build publishes a
// separate firmware binary so the F1 board doesn't accidentally pull
// it (and vice-versa).
#ifndef OTA_FIRMWARE_ASSET
#define OTA_FIRMWARE_ASSET "firmware-tictactoe.bin"
#endif
#ifndef OTA_VERSION_ASSET
#define OTA_VERSION_ASSET "version-tictactoe.txt"
#endif

#define OTA_RELEASE_TAG  "firmware-latest"
#define OTA_VERSION_URL  "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/" OTA_VERSION_ASSET
#define OTA_FIRMWARE_URL "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/" OTA_FIRMWARE_ASSET

namespace wifi_ota {

volatile bool inProgress = false;
bool          inApMode   = false;

bool connectWifi() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin();

  Serial.print("WiFi: connecting using saved credentials");
  rgb_panel::setBusy(0, true);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(200);
    if (millis() - start > 30000) {
      Serial.println("\nWiFi: STA timeout");
      rgb_panel::setBusy(0, false);
      return false;
    }
  }
  Serial.printf("\nWiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
  rgb_panel::setBusy(0, false);
  return true;
}

void beginWifiNonBlocking() {
  WiFi.persistent(true);
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin();
  Serial.println("WiFi: background associate started (non-blocking)");
}

static void runProvisioningPortal() {
  Serial.printf("WiFi: starting provisioning portal — join SSID '%s'"
                " (pwd '%s'), then visit http://192.168.4.1/\n",
                OTA_HOSTNAME, OTA_PASSWORD);

  rgb_panel::setAll(false, false, true);   // solid blue while portal up
  inApMode = true;

  WiFiManager wm;
  wm.setHostname(OTA_HOSTNAME);
  wm.setConfigPortalTimeout(300);
  wm.setConnectTimeout(30);
  wm.setBreakAfterConfig(true);

  bool ok = (strlen(OTA_PASSWORD) >= 8)
              ? wm.startConfigPortal(OTA_HOSTNAME, OTA_PASSWORD)
              : wm.startConfigPortal(OTA_HOSTNAME);

  if (!ok) {
    Serial.println("WiFi: portal timed out without valid creds, rebooting");
    delay(200);
    ESP.restart();
  }

  Serial.printf("WiFi: provisioned, IP=%s\n", WiFi.localIP().toString().c_str());
  inApMode = false;
  rgb_panel::blank();
}

void connectOrProvision(bool provisioningAllowed) {
  if (connectWifi()) { inApMode = false; return; }
  if (provisioningAllowed) { runProvisioningPortal(); return; }
  Serial.println("WiFi: provisioning portal not armed, restarting...");
  delay(200);
  ESP.restart();
}

void startProvisioningPortal() { runProvisioningPortal(); }

void eraseStoredCredentials() {
  Serial.println("WiFi: factory-reset — wiping saved STA credentials");
  WiFi.disconnect(true, true);
  WiFiManager wm;
  wm.resetSettings();
  delay(50);
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
      rgb_panel::showOtaProgress(1);
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
  IPAddress ip = inApMode ? WiFi.softAPIP() : WiFi.localIP();
  Serial.printf("OTA: ready at %s.local (%s%s)\n",
                OTA_HOSTNAME, ip.toString().c_str(),
                inApMode ? ", AP mode" : "");
}

void handleOta() { ArduinoOTA.handle(); }

#ifndef DISABLE_GITHUB_OTA
namespace {

constexpr int kErrBegin     = -1000;
constexpr int kErrEmptyBody = -1001;

String fetchLatestVersion(int *errOut) {
  *errOut = 0;
  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(10);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("f1-esp32-ota");
  if (!http.begin(client, OTA_VERSION_URL)) {
    Serial.println("OTA-check: http.begin failed");
    *errOut = kErrBegin;
    return String();
  }
  int code = http.GET();
  String body;
  if (code == HTTP_CODE_OK) {
    body = http.getString();
    body.trim();
    if (body.isEmpty()) *errOut = kErrEmptyBody;
  } else {
    Serial.printf("OTA-check: HTTP %d\n", code);
    *errOut = code;
  }
  http.end();
  return body;
}

}  // namespace

void checkAndUpdateFromGithub() {
  if (inApMode) {
    Serial.println("OTA-check: in AP fallback, skipping GitHub self-update");
    return;
  }
  Serial.printf("OTA-check: running version '%s'\n", FIRMWARE_VERSION);

  rgb_panel::setBusy(0, true);
  int fetchErr = 0;
  String latest = fetchLatestVersion(&fetchErr);
  rgb_panel::setBusy(0, false);

  if (latest.isEmpty()) {
    Serial.printf("OTA-check: could not read latest version (err=%d)\n", fetchErr);
    rgb_panel::setAll(true, false, false);   // solid red = fetch error
    return;
  }
  Serial.printf("OTA-check: latest = '%s'\n", latest.c_str());

  if (latest == FIRMWARE_VERSION) {
    Serial.println("OTA-check: already up to date");
    rgb_panel::setAll(false, true, false);   // solid green = up to date
    return;
  }

  Serial.println("OTA-check: new version available, downloading...");
  rgb_panel::showOtaProgress(1);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);

  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onStart([]()              { Serial.println("HTTPUpdate: start"); rgb_panel::showOtaProgress(1); });
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
    case HTTP_UPDATE_NO_UPDATES: Serial.println("HTTPUpdate: no updates"); break;
    case HTTP_UPDATE_OK:         Serial.println("HTTPUpdate: ok (rebooting)"); break;
  }
}
#else  // DISABLE_GITHUB_OTA
void checkAndUpdateFromGithub() {
  Serial.println("OTA-check: GitHub self-update disabled at build time");
  rgb_panel::setAll(true, true, false);   // yellow = lean build
}
#endif

}  // namespace wifi_ota

