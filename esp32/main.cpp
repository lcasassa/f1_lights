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
// trigger the factory-reset (wipe stored WiFi credentials).
static constexpr uint32_t kFactoryResetHoldMs = 10000;

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

  // Sample both buttons NOW (before WiFi association eats 30 s of wall
  // clock) to decide whether to allow the provisioning portal when the
  // saved credentials don't connect. Holding A+B at boot arms it; keep
  // them held for a further 10 s to also wipe the stored credentials.
  const bool provisioningAllowed = peripherals::bothButtonsPressed();
  if (provisioningAllowed) {
    Serial.println("boot: A+B held, provisioning portal armed");
    if (waitForFactoryResetHold()) {
      wifi_ota::eraseStoredCredentials();
      // Forced into the portal — there are no creds left to try.
    }
  }

  wifi_ota::connectOrProvision(provisioningAllowed);
  wifi_ota::checkAndUpdateFromGithub();
  wifi_ota::setupArduinoOta();

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

  // Auto-reconnect only in STA mode; the provisioning portal manages its
  // own lifecycle and a reconnect attempt would tear it down.
  if (!wifi_ota::inApMode && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: dropped, reconnecting...");
    wifi_ota::connectOrProvision(/*provisioningAllowed=*/false);
    wifi_ota::setupArduinoOta();
  }
}



