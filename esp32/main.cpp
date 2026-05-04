// f1_lights — ESP32-C3 firmware entry point.
//
// Hardware: HT16K33 backpack on I2C with 10 RGB LEDs piggy-backed on the
// 14-seg row lines, two 5-digit 7-seg modules sharing the same COMs, a
// passive buzzer, and two front-panel push buttons.
//
// Each "device" lives in its own module; main.cpp just wires them together.

#include <Arduino.h>
#include <WiFi.h>

#include "animation.h"
#include "ht16k33.h"
#include "peripherals.h"
#include "segment_scan.h"
#include "wifi_ota.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif

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
  animation::startupBlink();

  // Sample both buttons NOW (before WiFi association eats 30 s of wall
  // clock) to decide whether to allow the provisioning portal when the
  // saved credentials don't connect. Holding A+B at boot arms it.
  const bool provisioningAllowed = peripherals::bothButtonsPressed();
  if (provisioningAllowed) {
    Serial.println("boot: A+B held, provisioning portal armed if STA fails");
  }

  wifi_ota::connectOrProvision(provisioningAllowed);
  wifi_ota::checkAndUpdateFromGithub();
  wifi_ota::setupArduinoOta();
}

void loop() {
  wifi_ota::handleOta();

  if (!wifi_ota::inProgress) {
    animation::tick(peripherals::anyButtonPressed());
  }

  // Auto-reconnect only in STA mode; the provisioning portal manages its
  // own lifecycle and a reconnect attempt would tear it down.
  if (!wifi_ota::inApMode && WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: dropped, reconnecting...");
    wifi_ota::connectOrProvision(/*provisioningAllowed=*/false);
    wifi_ota::setupArduinoOta();
  }
}



