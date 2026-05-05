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
static constexpr uint32_t kWifiArmHoldMs = 1000;

// Both buttons must stay pressed continuously for this long at boot to
// also wipe the stored WiFi credentials and force the provisioning
// portal (factory reset). Same hold, longer duration.
static constexpr uint32_t kFactoryResetHoldMs = 5000;

// Max time we'll spend on the blocking association attempt during the
// armed boot path before declaring failure and dropping into the portal.
static constexpr uint32_t kArmedConnectTimeoutMs = 30000;

// Set in setup() based on the boot-time A+B hold; controls whether
// loop() is allowed to make blocking reconnect calls.
static bool g_wifiArmedBoot = false;
// True once setupArduinoOta() has been called against the current WiFi
// session. Cleared on disconnect; re-armed once we re-associate.
static bool g_otaReady      = false;

// Phase 1 of the boot-time A+B hold: silent 1 s wait — no LED feedback,
// just poll the buttons. Returns the total time A+B were held. If the
// user releases before kWifiArmHoldMs the caller treats it as "not
// armed" and skips the rest.
static uint32_t bootRampPhase() {
  Serial.println("boot: A+B held — silent 1 s wait, release before that for game-only mode");
  const uint32_t start = millis();
  while (peripherals::bothButtonsPressed() &&
         (millis() - start) < kWifiArmHoldMs) {
    delay(20);
  }
  return millis() - start;
}

// Phase 2 of the boot-time A+B hold (only entered after the 1 s
// threshold). Tries to associate with stored credentials in the
// background while:
//   - blinking yellow on every LED to indicate "connecting…"
//   - holding solid green once associated
//   - watching A+B; if held continuously to kFactoryResetHoldMs total
//     since boot start, abort and signal factory-reset (this check
//     wins over a successful WiFi connect, so the user can always
//     force the portal by just keeping the buttons down)
// Returns one of:
//   ARMED_CONNECTED   WiFi up + buttons released, continue with OTA setup
//   ARMED_FAILED      solid red flashed, no WiFi, drop into portal
//   FACTORY_RESET     blue blinks, wipe creds + drop into portal
enum class BootOutcome { ARMED_CONNECTED, ARMED_FAILED, FACTORY_RESET };

static BootOutcome bootArmedPhase(uint32_t holdStartMs) {
  Serial.println("boot: 1 s threshold — yellow blink, attempting WiFi connect "
                 "(keep holding 5 s total to factory-reset)");
  // Kick off association without blocking; we'll poll WiFi.status().
  wifi_ota::beginWifiNonBlocking();

  const uint32_t connectStart = millis();
  uint32_t lastBlinkMs = 0;
  bool blinkOn = false;
  bool connected = false;

  while (true) {
    const uint32_t now = millis();

    // Factory-reset has priority: A+B held continuously since holdStartMs
    // for >=5 s wins even if WiFi has already associated. Any release
    // since holdStartMs cancels it (peripherals::bothButtonsPressed()
    // is the live state, so a release here breaks the hold streak).
    if (peripherals::bothButtonsPressed() &&
        (now - holdStartMs) >= kFactoryResetHoldMs) {
      Serial.println("boot: held >=5 s → factory reset path "
                     "(overrides WiFi-connected outcome)");
      rgb_panel::blank();
      return BootOutcome::FACTORY_RESET;
    }

    // Detect first transition to associated → switch ring to solid green
    // and stop the yellow blink. We then keep polling for either button
    // release (→ ARMED_CONNECTED) or the 5 s factory-reset threshold.
    if (!connected && WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.printf("boot: WiFi connected, IP=%s\n",
                    WiFi.localIP().toString().c_str());
      rgb_panel::setAll(false, true, false);   // solid green
    }

    if (connected) {
      // Wait for the user to let go before committing to the connected
      // outcome. While they keep holding, the factory-reset check above
      // is still ticking.
      if (!peripherals::bothButtonsPressed()) {
        delay(200);                            // brief green dwell
        rgb_panel::blank();
        return BootOutcome::ARMED_CONNECTED;
      }
      delay(10);
      continue;
    }

    // Still connecting: yellow heartbeat (~4 Hz) + 30 s timeout.
    if (now - connectStart >= kArmedConnectTimeoutMs) {
      Serial.println("boot: WiFi connect timed out → portal");
      rgb_panel::setAll(true, false, false);  // red
      delay(600);
      rgb_panel::blank();
      return BootOutcome::ARMED_FAILED;
    }
    if (now - lastBlinkMs >= 125) {
      lastBlinkMs = now;
      blinkOn = !blinkOn;
      if (blinkOn) rgb_panel::setAll(true, true, false);  // yellow
      else         rgb_panel::blank();
    }
    delay(10);
  }
}

// Short blue-blink confirmation that we're committing to the portal
// (called for both FACTORY_RESET and the post-failure portal entry).
// In the factory-reset case we tear down any pending STA association
// FIRST so the portal comes up cold — no lingering WiFi.begin() retry,
// no "try with old creds one more time", just the AP.
static void bootEnterPortal(bool eraseCreds) {
  if (eraseCreds) {
    // Cancel the background associate kicked off by bootArmedPhase
    // before we even start the blink, so the radio isn't fighting us.
    WiFi.disconnect(/*wifioff=*/true, /*eraseap=*/false);
    Serial.println("boot: factory-reset path — aborting any pending STA, going straight to portal");
  }
  if (eraseCreds) {
    wifi_ota::eraseStoredCredentials();
  }
  // runProvisioningPortal calls WiFiManager::startConfigPortal() which
  // brings up the SoftAP directly — it does NOT try to associate first
  // (that would require autoConnect()). So with creds wiped above, this
  // path is guaranteed to land in the portal without any STA attempt.
  wifi_ota::startProvisioningPortal();
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
  // wall clock) and run the two-phase A+B hold sequence:
  //   phase 1 (0..1 s):  silent wait; release ends boot decision
  //   phase 2 (1 s..):   yellow blink while WiFi associates in the
  //                      background; green = success, red = timeout,
  //                      5 s total hold = blue blink + factory-reset
  if (peripherals::bothButtonsPressed()) {
    const uint32_t holdStart = millis();
    const uint32_t totalHeld = bootRampPhase();
    if (totalHeld < kWifiArmHoldMs) {
      // Released before the arm threshold → game-only mode.
      g_wifiArmedBoot = false;
    } else {
      g_wifiArmedBoot = true;
      switch (bootArmedPhase(holdStart)) {
        case BootOutcome::ARMED_CONNECTED:
          // WiFi up — finish OTA boot path below.
          break;
        case BootOutcome::ARMED_FAILED:
          // STA timed out → portal (no creds wipe).
          bootEnterPortal(/*eraseCreds=*/false);
          break;
        case BootOutcome::FACTORY_RESET:
          // 5 s hold → wipe creds + portal.
          bootEnterPortal(/*eraseCreds=*/true);
          break;
      }
    }
  }

  if (g_wifiArmedBoot) {
    // After bootArmedPhase() / bootEnterPortal() we either have an STA
    // association (CONNECTED) or are coming back from the portal also
    // associated. Either way: do the GitHub self-update check and arm
    // ArduinoOTA.
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



