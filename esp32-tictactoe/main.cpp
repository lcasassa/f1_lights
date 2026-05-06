// f1_lights tic-tac-toe — ESP32-C3 firmware entry point.
//
// Hardware: HT16K33 backpack on I2C (SDA=GPIO4, SCL=GPIO5) with a
// single RGB LED on the 14-seg digit at COM 4 (R=G2, G=D, B=DP). No
// front-panel buttons, no 7-seg displays, no buzzer (yet).
//
// Boot flow:
//   1. Bring up HT16K33 + RGB LED.
//   2. Quick startup blink.
//   3. Try a short blocking WiFi associate (kBootConnectWaitMs) and,
//      if it succeeds, run the GitHub self-update check + arm
//      ArduinoOTA. Otherwise fall back to a non-blocking associate
//      that loop() will finish when the radio comes up.
//   4. loop(): handle OTA, drive the idle animation, keep WiFi alive.

#include <Arduino.h>
#include <WiFi.h>

#include "animation.h"
#include "ht16k33.h"
#include "peripherals.h"
#include "rgb_panel.h"
#include "wifi_ota.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif

// How long setup() will block waiting for WiFi at boot before giving
// up and continuing in non-blocking mode. Long enough to let the
// GitHub self-update check run on a normal home network, short enough
// that an offline board still reaches loop() quickly.
static constexpr uint32_t kBootConnectWaitMs = 15000;

// True once setupArduinoOta() has been called against the current
// WiFi session. Cleared on disconnect; re-armed once we re-associate.
static bool g_otaReady = false;

// Block up to `timeoutMs` for STA association. Returns true if
// connected before the deadline. Polls every 200 ms; no UI on the
// single LED (we want it free for the OTA progress indicator).
static bool waitForWifi(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) return false;
    delay(200);
  }
  return true;
}

// Maintain WiFi + ArduinoOTA in the background. Mirrors the
// non-armed branch from the F1 build.
static void maintainWifiAndOta() {
  if (wifi_ota::inApMode) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!g_otaReady) {
      Serial.printf("WiFi: connected (background), IP=%s\n",
                    WiFi.localIP().toString().c_str());
      wifi_ota::setupArduinoOta();
      g_otaReady = true;
    }
    return;
  }

  // Disconnected: retry begin every 10 s.
  g_otaReady = false;
  static uint32_t s_lastBeginMs = 0;
  const uint32_t nowMs = millis();
  if (s_lastBeginMs == 0 || nowMs - s_lastBeginMs > 10000) {
    s_lastBeginMs = nowMs;
    wifi_ota::beginWifiNonBlocking();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-tictactoe boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  // 1. Bring up hardware modules.
  peripherals::setupLed();
  peripherals::setupBuzzer();
  ht16k33::setup();

  // 2. Boot self-test flash.
  animation::startupBlink();

  // 3. Try a short blocking WiFi associate so the GitHub self-updater
  //    has a chance to run on the very first boot. If it fails, fall
  //    back to background association — the user can recover by
  //    bringing up the network and rebooting.
  wifi_ota::beginWifiNonBlocking();
  if (waitForWifi(kBootConnectWaitMs)) {
    Serial.printf("WiFi: connected at boot, IP=%s\n",
                  WiFi.localIP().toString().c_str());
    wifi_ota::checkAndUpdateFromGithub();
    wifi_ota::setupArduinoOta();
    g_otaReady = true;
  } else {
    Serial.println("WiFi: not up within boot window, continuing offline");
    g_otaReady = false;
  }

  // 4. Clear boot diagnostics so the idle animation starts clean.
  rgb_panel::blank();
}

// Toggle the onboard LED at ~2 Hz so it's obvious from across the room
// that loop() is actually running, even if the HT16K33-driven RGB LED
// stays dark for any reason (bad solder, wrong digit, etc.).
static void blinkOnboardLedDebug() {
  static uint32_t s_lastMs = 0;
  static bool     s_on     = false;
  const uint32_t now = millis();
  if (now - s_lastMs < 250) return;
  s_lastMs = now;
  s_on = !s_on;
  // Onboard LED on the ESP32-C3 Super Mini is active-LOW.
  digitalWrite(ONBOARD_LED_PIN, s_on ? LOW : HIGH);
}

void loop() {
  // 0. Heartbeat: blink the onboard LED so we know loop() is alive.
  blinkOnboardLedDebug();

  // 1. Service any in-flight OTA upload.
  wifi_ota::handleOta();

  // 2. Idle animation — but suppress it while OTA is uploading so the
  //    LED can show progress.
  if (!wifi_ota::inProgress) animation::tick();

  // 3. Keep WiFi + ArduinoOTA healthy in the background.
  maintainWifiAndOta();
}


