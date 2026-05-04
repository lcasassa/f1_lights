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
  wifi_ota::connectWifi();
  wifi_ota::checkAndUpdateFromGithub();   // may reboot into new firmware
  wifi_ota::setupArduinoOta();
  animation::startupBlink();
}

void loop() {
  wifi_ota::handleOta();

  if (!wifi_ota::inProgress) {
    animation::tick(peripherals::anyButtonPressed());
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: dropped, reconnecting...");
    wifi_ota::connectWifi();
    wifi_ota::setupArduinoOta();
  }
}

