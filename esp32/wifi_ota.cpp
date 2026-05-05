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
#include "seg7.h"
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
bool          inApMode   = false;

bool connectWifi() {
  WiFi.persistent(true);          // store SSID/PSK in NVS for next boot
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin();                   // uses last-saved creds from NVS

  Serial.print("WiFi: connecting using saved credentials");
  rgb_panel::setBusy(0, true);    // LED #1: WiFi associate in progress
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
  WiFi.begin();                   // returns immediately; association runs in background
  Serial.println("WiFi: background associate started (non-blocking)");
}

// Bring up the SoftAP + captive-portal webserver so the user can pick an
// SSID and type the password. Blocks until the user submits valid creds
// (which WiFiManager then writes to NVS) or the portal times out.
static void runProvisioningPortal() {
  Serial.printf("WiFi: starting provisioning portal — join SSID '%s'"
                " (pwd '%s'), then visit http://192.168.4.1/\n",
                OTA_HOSTNAME, OTA_PASSWORD);

  // Visual cue: all RGB LEDs solid blue while the portal is open.
  rgb_panel::setAll(false, false, true);
  inApMode = true;

  WiFiManager wm;
  wm.setHostname(OTA_HOSTNAME);
  wm.setConfigPortalTimeout(300);   // 5 min, then reboot
  wm.setConnectTimeout(30);         // attempt-to-join timeout per submitted creds
  wm.setBreakAfterConfig(true);     // exit portal as soon as creds are saved

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
  if (connectWifi()) {
    inApMode = false;
    return;
  }
  if (provisioningAllowed) {
    runProvisioningPortal();
    return;
  }
  Serial.println("WiFi: provisioning portal not armed, restarting...");
  delay(200);
  ESP.restart();
}

void eraseStoredCredentials() {
  Serial.println("WiFi: factory-reset — wiping saved STA credentials");
  // IDF-side STA SSID/PSK in NVS. Second `true` = eraseAP (NVS).
  WiFi.disconnect(true, true);
  // WiFiManager-side cached creds (its own NVS namespace).
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
  IPAddress ip = inApMode ? WiFi.softAPIP() : WiFi.localIP();
  Serial.printf("OTA: ready at %s.local (%s%s)\n",
                OTA_HOSTNAME, ip.toString().c_str(),
                inApMode ? ", AP mode" : "");
}

void handleOta() {
  ArduinoOTA.handle();
}

#ifndef DISABLE_GITHUB_OTA
namespace {

// Out-param error codes: 0 = success, positive = HTTP status (e.g. 404),
// negative = HTTPClient transport error (e.g. -11 read timeout, -1 conn
// refused). Special pre-HTTP codes:
constexpr int kErrBegin     = -1000;  // http.begin() failed (URL parse / TLS init)
constexpr int kErrEmptyBody = -1001;  // 200 OK but body was empty after trim

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
    *errOut = code;   // negative for HTTPClient errors, positive for HTTP status
  }
  http.end();
  return body;
}

// Render an error on the front panel: "Err" on the top 7-seg, the
// numeric error code on the bottom one, and the full RGB ring red.
void showFetchError(int code) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%d", code);
  seg7::writeText(seg7::kTop, "Err");
  seg7::writeText(seg7::kBot, buf);
  rgb_panel::setAll(true, false, false);   // all LEDs red
  Serial.printf("OTA-check: error code %d shown on panel\n", code);
}

}  // namespace

void checkAndUpdateFromGithub() {
  if (inApMode) {
    Serial.println("OTA-check: in AP fallback, skipping GitHub self-update");
    return;
  }
  Serial.printf("OTA-check: running version '%s'\n", FIRMWARE_VERSION);

  // While we're talking to GitHub (DNS + TLS handshake + HTTP GET), there's
  // no real % to show — light LED #2 as a "busy" indicator.
  rgb_panel::setBusy(1, true);
  int fetchErr = 0;
  String latest = fetchLatestVersion(&fetchErr);
  rgb_panel::setBusy(1, false);

  if (latest.isEmpty()) {
    Serial.println("OTA-check: could not read latest version, skipping");
    showFetchError(fetchErr);
    return;
  }
  Serial.printf("OTA-check: latest = '%s'\n", latest.c_str());

  if (latest == FIRMWARE_VERSION) {
    Serial.println("OTA-check: already up to date");
    rgb_panel::setBusy(9, true);   // LED #10: "running latest" indicator
    return;
  }

  // Out of date — show the *new* version (truncated to 5 chars to fit the
  // 5-cell panel; the seg7 encoder handles 0-9 + a-f). Top display only;
  // the bottom one stays blank so the new SHA is visually unambiguous.
  String latestShort = latest.substring(0, 5);
  seg7::writeText(seg7::kTop, latestShort.c_str());
  seg7::clear(seg7::kBot);

  Serial.println("OTA-check: new version available, downloading...");
  // showOtaProgress will paint the RGB ring during the download; the
  // seg7 SHAs stay visible because they own disjoint segment bits.
  rgb_panel::showOtaProgress(0);

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);

  // GitHub release assets reply with a 302 to objects.githubusercontent.com,
  // so we need httpUpdate to follow redirects (it doesn't by default).
  httpUpdate.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
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
  // Visual cue so it's obvious from the panel that this is the lean
  // build (no fetch, no version compare). LED #5 yellow = "skipped".
  rgb_panel::setLed(4, true, true, false);
}
#endif

}  // namespace wifi_ota






