// f1_lights — ESP32-C3 firmware entry point.
//
// Hardware: HT16K33 backpack on I2C with 10 RGB LEDs piggy-backed on the
// 14-seg row lines, two 5-digit 7-seg modules sharing the same COMs, a
// passive buzzer, and two front-panel push buttons.
//
// Each "device" lives in its own module; main.cpp just wires them together.

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include "animation.h"
#include "ht16k33.h"
#include "peripherals.h"
#include "rgb_panel.h"
#include "seg7.h"
#include "segment_scan.h"
#include "wifi_ota.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif

// Both buttons must stay pressed continuously for this long at boot to
// arm the blocking WiFi+OTA path (otherwise WiFi connects in the
// background and the game starts immediately).
static constexpr uint32_t kWifiArmHoldMs = 2000;

// Both buttons must stay pressed continuously for this long at boot to
// trigger the factory-reset (wipe stored WiFi credentials). Counted
// from when the WiFi-arm threshold is reached.
static constexpr uint32_t kFactoryResetHoldMs = 10000;

// Set in setup() based on the boot-time A+B hold; controls whether
// loop() is allowed to make blocking reconnect calls.
static bool g_wifiArmedBoot = false;
// True once setupArduinoOta() has been called against the current WiFi
// session. Cleared on disconnect; re-armed once we re-associate.
static bool g_otaReady      = false;

// Block until either both buttons are released or the hold deadline is
// reached. Returns true iff the user held A+B for the full duration.
// The RGB ring fills red as a visible countdown.
static bool waitForFactoryResetHold() {
  Serial.println("boot: keep A+B held for 10 s for factory reset...");
  const uint32_t start = millis();
  while (peripherals::bothButtonsPressed()) {
    uint32_t elapsed = millis() - start;
    if (elapsed >= kFactoryResetHoldMs) {
      rgb_panel::showOtaProgress(100);   // all-red = committed
      return true;
    }
    int pct = (int)((uint64_t)elapsed * 100 / kFactoryResetHoldMs);
    rgb_panel::showOtaProgress(pct);
    delay(50);
  }
  rgb_panel::blank();
  Serial.println("boot: A+B released early, factory reset cancelled");
  return false;
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-esp32 boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  peripherals::setupLed();
  peripherals::setupBuzzer();
  peripherals::setupButtons();
  ht16k33::setup();

#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)
  segment_scan::run();          // never returns
#endif

  animation::startupBlink();

  // Sample both buttons NOW (before any potential WiFi association eats
  // wall clock) to decide the boot path:
  //
  //   not held       → background WiFi connect, game starts immediately
  //   held >= 2 s    → blocking WiFi connect + provisioning portal armed
  //                    + GitHub self-update check
  //   held >= 12 s   → also wipe stored WiFi credentials (factory reset)
  if (peripherals::bothButtonsPressed()) {
    Serial.println("boot: A+B held — keep holding 2 s to arm WiFi/OTA path");
    const uint32_t armStart = millis();
    while (peripherals::bothButtonsPressed() &&
           millis() - armStart < kWifiArmHoldMs) {
      // Visual ramp on the RGB ring while the user makes up their mind.
      int pct = (int)((uint64_t)(millis() - armStart) * 100 / kWifiArmHoldMs);
      rgb_panel::showOtaProgress(pct);
      delay(20);
    }
    if (peripherals::bothButtonsPressed()) {
      g_wifiArmedBoot = true;
      Serial.println("boot: WiFi armed (blocking connect + provisioning portal)");
      if (waitForFactoryResetHold()) {
        wifi_ota::eraseStoredCredentials();
      }
    } else {
      rgb_panel::blank();
      Serial.println("boot: A+B released before 2 s, WiFi will connect in background");
    }
  }

  if (g_wifiArmedBoot) {
    wifi_ota::connectOrProvision(/*provisioningAllowed=*/true);
    wifi_ota::checkAndUpdateFromGithub();
    wifi_ota::setupArduinoOta();
    g_otaReady = true;
  } else {
    // Non-blocking: kick off association and let loop() finish the OTA
    // setup once WL_CONNECTED arrives. Game starts now.
    wifi_ota::beginWifiNonBlocking();
    g_otaReady = false;
  }

  // Boot diagnostics done — clear every status LED + display so the loop
  // starts from a clean slate. Anything still on at this point (LED #10
  // up-to-date, LED #5 lean-build, "Err"+code panel, …) was just a
  // boot-time signal; the running device shouldn't keep showing it.
  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

void loop() {
  static uint32_t s_lastActivityMs = 0;
  static bool     s_activitySeeded = false;
  constexpr uint32_t kIdleSleepMs = 60000;   // 1 minute

  if (!s_activitySeeded) {
    s_lastActivityMs = millis();   // setup() may have taken >60 s (WiFi + OTA fetch)
    s_activitySeeded = true;
  }

  wifi_ota::handleOta();

  if (!wifi_ota::inProgress) {
    const bool a = peripherals::buttonA();
    const bool b = peripherals::buttonB();
    animation::tick(a, b);

    const uint32_t now = millis();
    if (a || b) {
      s_lastActivityMs = now;
    } else if (!wifi_ota::inApMode &&
               (now - s_lastActivityMs) >= kIdleSleepMs) {
      // Idle too long → light sleep until either button is pressed.
      // Light sleep keeps RAM + program state intact; the OTA receiver
      // and game state machine pick up exactly where they left off.
      Serial.println("idle: 60 s without input, entering light sleep");
      Serial.flush();
      rgb_panel::blank();
      seg7::clear(seg7::kTop);
      seg7::clear(seg7::kBot);

      // Buttons use INPUT_PULLUP, so pressed = LOW. Wake on low level.
      gpio_wakeup_enable((gpio_num_t)BTN_A_PIN, GPIO_INTR_LOW_LEVEL);
      gpio_wakeup_enable((gpio_num_t)BTN_B_PIN, GPIO_INTR_LOW_LEVEL);
      esp_sleep_enable_gpio_wakeup();

      esp_light_sleep_start();

      Serial.println("idle: woke up from light sleep");
      s_lastActivityMs = millis();
    }
  } else {
    s_lastActivityMs = millis();   // OTA traffic counts as activity
  }

  // WiFi maintenance — strategy depends on the boot-time arm flag.
  //   armed:    blocking reconnect on disconnect (preserves OTA upload UX)
  //   not armed: non-blocking begin, retry every 10 s; setupArduinoOta()
  //             is deferred until association actually completes.
  if (!wifi_ota::inApMode) {
    const bool connected = (WiFi.status() == WL_CONNECTED);
    if (!connected) {
      g_otaReady = false;
      if (g_wifiArmedBoot) {
        Serial.println("WiFi: dropped, reconnecting (blocking)...");
        wifi_ota::connectOrProvision(/*provisioningAllowed=*/false);
        wifi_ota::setupArduinoOta();
        g_otaReady = true;
      } else {
        static uint32_t s_lastBeginMs = 0;
        const uint32_t nowMs = millis();
        if (s_lastBeginMs == 0 || nowMs - s_lastBeginMs > 10000) {
          s_lastBeginMs = nowMs;
          wifi_ota::beginWifiNonBlocking();
        }
      }
    } else if (!g_otaReady) {
      // Non-blocking path just associated — finish OTA setup now.
      Serial.printf("WiFi: connected (background), IP=%s\n",
                    WiFi.localIP().toString().c_str());
      wifi_ota::setupArduinoOta();
      g_otaReady = true;
    }
  }
}



