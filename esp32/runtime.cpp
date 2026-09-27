// See runtime.h. This file owns the small bits of state that need to
// live between loop() calls (OTA-ready flag, last-activity timestamp,
// last non-blocking begin attempt, …) so main.cpp doesn't have to.

#include "runtime.h"

#include <Arduino.h>
#include <WiFi.h>
#include <esp_sleep.h>
#include <driver/gpio.h>

#include "animation.h"
#include "boot_sequence.h"
#include "peripherals.h"
#include "wifi_ota.h"

namespace {

// Idle threshold for the light-sleep path.
constexpr uint32_t kIdleSleepMs = 60000;   // 1 minute

// True once setupArduinoOta() has been called against the current WiFi
// session. Cleared on disconnect; re-armed once we re-associate.
bool g_otaReady = false;

// Configure the two front-panel buttons as wakeup sources and enter
// light sleep. RAM + program state are preserved, so the OTA receiver
// and game state machine resume exactly where they left off.
// Buttons use INPUT_PULLUP, so pressed = LOW.
void enterLightSleepUntilButton() {
  Serial.println("idle: 60 s without input, entering light sleep");
  Serial.flush();
  boot_sequence::clearDiagnostics();

  gpio_wakeup_enable((gpio_num_t)BTN_A_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_B_PIN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  esp_light_sleep_start();

  Serial.println("idle: woke up from light sleep");
}

// Drive the game animation for one tick and report whether either
// button was active. While OTA is uploading we suppress the animation
// (the bus needs to be free) and treat OTA traffic itself as activity.
bool runGameTick() {
  if (wifi_ota::inProgress) return true;
  const bool a = peripherals::buttonA();
  const bool b = peripherals::buttonB();
  animation::tick(a, b);
  return a || b;
}

// Idle-tracking helper. setup() can take >60 s (WiFi association + GitHub
// OTA fetch), so we lazily seed the "last activity" timestamp on the first
// loop() iteration instead of letting it default to 0 — otherwise we'd
// drop straight into light sleep before the user got a chance to play.
// Returns the number of milliseconds since the last reported activity.
uint32_t idleMillisSinceActivity(bool active) {
  static uint32_t s_lastActivityMs = 0;
  static bool     s_seeded         = false;
  const uint32_t now = millis();
  if (!s_seeded) {
    s_lastActivityMs = now;
    s_seeded = true;
  }
  if (active) s_lastActivityMs = now;
  return now - s_lastActivityMs;
}

// WiFi maintenance — strategy depends on the boot-time arm flag.
//   armed:     blocking reconnect on disconnect (preserves OTA upload UX).
//   not armed: non-blocking begin, retry every 10 s; setupArduinoOta()
//              is deferred until association actually completes.
void maintainWifiAndOta(bool wifiArmedBoot) {
  if (wifi_ota::inApMode) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!g_otaReady) {
      // Non-blocking path just associated — finish OTA setup now.
      Serial.printf("WiFi: connected (background), IP=%s\n",
                    WiFi.localIP().toString().c_str());
      wifi_ota::setupArduinoOta();
      g_otaReady = true;
    }
    return;
  }

  // Disconnected.
  g_otaReady = false;
  if (wifiArmedBoot) {
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
}

}  // namespace

namespace runtime {

void loopTick(bool wifiArmedBoot) {
  // 1. Service any in-flight OTA upload.
  wifi_ota::handleOta();

  // 2. Run the game (or skip it while OTA is uploading); if no input
  //    has happened for kIdleSleepMs, drop into light sleep until a
  //    button wakes us. Skip sleep while the provisioning portal is up.
  const bool active = runGameTick();
  if (!wifi_ota::inApMode &&
      idleMillisSinceActivity(active) >= kIdleSleepMs) {
    enterLightSleepUntilButton();
    (void)idleMillisSinceActivity(/*active=*/true);   // reseed post-wake
  }

  // 3. Keep WiFi + ArduinoOTA healthy in the background.
  maintainWifiAndOta(wifiArmedBoot);
}

// Exposed so main.cpp's setup() can flip the flag once it has finished
// the synchronous OTA bring-up on the armed path. Defined out-of-line
// so the header doesn't need to know about the storage.
void markOtaReady() { g_otaReady = true; }
void clearOtaReady() { g_otaReady = false; }

}  // namespace runtime

