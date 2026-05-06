// f1_lights — ESP32-C3 firmware entry point.
//
// Hardware: HT16K33 backpack on I2C with 10 RGB LEDs piggy-backed on the
// 14-seg row lines, two 5-digit 7-seg modules sharing the same COMs, a
// passive buzzer, and two front-panel push buttons.
//
// Each "device" lives in its own module; main.cpp just wires them
// together. setup() and loop() are kept intentionally short — they read
// as a sequence of named steps; the details live in the static helpers
// further down (boot button hold, WiFi maintenance, idle sleep).

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

// ---------------------------------------------------------------------------
// Boot-time A+B hold thresholds
// ---------------------------------------------------------------------------
// Both buttons must stay pressed continuously at boot for these durations:
//   <  1 s : game-only mode, WiFi connects in the background.
//   >= 1 s : "armed" — block boot on WiFi + OTA self-update.
//   >= 5 s : factory reset — wipe stored credentials, force the portal.
static constexpr uint32_t kWifiArmHoldMs        = 1000;
static constexpr uint32_t kFactoryResetHoldMs   = 5000;
// Max time we'll spend on the blocking association attempt during the
// armed boot path before declaring failure and dropping into the portal.
static constexpr uint32_t kArmedConnectTimeoutMs = 30000;

// Idle threshold for loop()'s light-sleep path.
static constexpr uint32_t kIdleSleepMs = 60000;   // 1 minute

// ---------------------------------------------------------------------------
// Module-local state shared between setup() and loop()
// ---------------------------------------------------------------------------
// Set in setup() based on the boot-time A+B hold; controls whether
// loop() is allowed to make blocking reconnect calls.
static bool g_wifiArmedBoot = false;
// True once setupArduinoOta() has been called against the current WiFi
// session. Cleared on disconnect; re-armed once we re-associate.
static bool g_otaReady      = false;

// ===========================================================================
// Boot-time A+B hold sequence
// ===========================================================================

enum class BootOutcome { ARMED_CONNECTED, ARMED_FAILED, FACTORY_RESET };

// Phase 1: silent 1 s wait. No LED feedback, just poll the buttons.
// Returns the total time A+B were held; if shorter than kWifiArmHoldMs
// the caller treats it as "not armed" and skips phase 2.
static uint32_t bootHoldPhase1Silent() {
  Serial.println("boot: A+B held — silent 1 s wait, release before that for game-only mode");
  const uint32_t start = millis();
  while (peripherals::bothButtonsPressed() &&
         (millis() - start) < kWifiArmHoldMs) {
    delay(20);
  }
  return millis() - start;
}

// Phase 2 (entered after the 1 s threshold). Tries to associate with
// stored credentials in the background while:
//   - blinking yellow on every LED to indicate "connecting…"
//   - holding solid green once associated
//   - watching A+B; if held continuously to kFactoryResetHoldMs total
//     since boot start, abort and signal factory-reset (this check
//     wins over a successful WiFi connect, so the user can always
//     force the portal by just keeping the buttons down).
static BootOutcome bootHoldPhase2Armed(uint32_t holdStartMs) {
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

    // First transition to associated → switch ring to solid green and
    // stop the yellow blink. Keep polling for either button release
    // (→ ARMED_CONNECTED) or the 5 s factory-reset threshold above.
    if (!connected && WiFi.status() == WL_CONNECTED) {
      connected = true;
      Serial.printf("boot: WiFi connected, IP=%s\n",
                    WiFi.localIP().toString().c_str());
      rgb_panel::setAll(false, true, false);   // solid green
    }

    if (connected) {
      // Wait for the user to let go before committing. While they keep
      // holding, the factory-reset check above is still ticking.
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

// Commit to the captive-portal path. For factory-reset we tear down any
// pending STA association FIRST so the portal comes up cold — no
// lingering WiFi.begin() retry, no "try with old creds one more time",
// just the AP. startProvisioningPortal() calls
// WiFiManager::startConfigPortal() which brings up the SoftAP directly
// (it does NOT try to associate first), so with creds wiped above this
// path is guaranteed to land in the portal without any STA attempt.
static void bootEnterPortal(bool eraseCreds) {
  if (eraseCreds) {
    WiFi.disconnect(/*wifioff=*/true, /*eraseap=*/false);
    Serial.println("boot: factory-reset path — aborting any pending STA, going straight to portal");
    wifi_ota::eraseStoredCredentials();
  }
  wifi_ota::startProvisioningPortal();
}

// Run the full A+B boot sequence (only called when both buttons are
// already down at boot). Sets g_wifiArmedBoot; on the armed paths it
// either returns with WiFi associated or routes through the portal.
static void runBootButtonSequence() {
  const uint32_t holdStart = millis();
  const uint32_t totalHeld = bootHoldPhase1Silent();
  if (totalHeld < kWifiArmHoldMs) {
    g_wifiArmedBoot = false;          // released early → game-only mode
    return;
  }
  g_wifiArmedBoot = true;
  switch (bootHoldPhase2Armed(holdStart)) {
    case BootOutcome::ARMED_CONNECTED:
      // WiFi up — finish OTA boot path in setup().
      break;
    case BootOutcome::ARMED_FAILED:
      bootEnterPortal(/*eraseCreds=*/false);   // STA timed out → portal
      break;
    case BootOutcome::FACTORY_RESET:
      bootEnterPortal(/*eraseCreds=*/true);    // 5 s hold → wipe + portal
      break;
  }
}

// Clear every status LED + display so the running game starts from a
// clean slate. Anything still on at this point (LED #10 up-to-date,
// LED #5 lean-build, "Err"+code panel, …) was just a boot-time signal.
static void clearBootDiagnostics() {
  rgb_panel::blank();
  seg7::clear(seg7::kTop);
  seg7::clear(seg7::kBot);
}

// ===========================================================================
// Loop helpers: idle sleep + WiFi maintenance
// ===========================================================================

// Configure the two front-panel buttons as wakeup sources and enter
// light sleep. RAM + program state are preserved, so the OTA receiver
// and game state machine resume exactly where they left off.
// Buttons use INPUT_PULLUP, so pressed = LOW.
static void enterLightSleepUntilButton() {
  Serial.println("idle: 60 s without input, entering light sleep");
  Serial.flush();
  clearBootDiagnostics();

  gpio_wakeup_enable((gpio_num_t)BTN_A_PIN, GPIO_INTR_LOW_LEVEL);
  gpio_wakeup_enable((gpio_num_t)BTN_B_PIN, GPIO_INTR_LOW_LEVEL);
  esp_sleep_enable_gpio_wakeup();

  esp_light_sleep_start();

  Serial.println("idle: woke up from light sleep");
}

// Drive the game animation for one tick and report whether either
// button was active. While OTA is uploading we suppress the animation
// (the bus needs to be free) and treat OTA traffic itself as activity.
static bool runGameTick() {
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
static uint32_t idleMillisSinceActivity(bool active) {
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
static void maintainWifiAndOta() {
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
}

// ===========================================================================
// Arduino entry points
// ===========================================================================

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-esp32 boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  // 1. Bring up hardware modules.
  peripherals::setupLed();
  peripherals::setupBuzzer();
  peripherals::setupButtons();
  ht16k33::setup();

#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)
  segment_scan::run();          // bench tool — never returns
#endif

  // 2. Boot self-test flash.
  animation::startupBlink();

  // 3. Sample A+B *now* (before any potential WiFi association eats
  //    wall clock) and run the two-phase boot button sequence.
  if (peripherals::bothButtonsPressed()) {
    runBootButtonSequence();
  }

  // 4. Bring WiFi + OTA online according to the boot decision.
  if (g_wifiArmedBoot) {
    // We have an STA association (either from the armed phase or via
    // the portal). Do the GitHub self-update check and arm ArduinoOTA.
    wifi_ota::checkAndUpdateFromGithub();
    wifi_ota::setupArduinoOta();
    g_otaReady = true;
  } else {
    // Game-only boot: kick off association in the background; loop()
    // will finish OTA setup once WL_CONNECTED arrives.
    wifi_ota::beginWifiNonBlocking();
    g_otaReady = false;
  }

  // 5. Clear boot diagnostics so the game starts on a clean slate.
  clearBootDiagnostics();
}

void loop() {
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
  maintainWifiAndOta();
}
