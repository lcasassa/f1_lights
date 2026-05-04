// OTA / provisioning identity for the ESP32-C3 build.
//
// Copy this file to `wifi_credentials.h` (same directory). The real file
// is git-ignored. WiFi SSID + password for the user's network are NOT
// stored here — they're entered through the on-device provisioning
// portal (AP fallback when both buttons are held at boot) and persisted
// to NVS. See README for details.
#pragma once
// mDNS hostname used for OTA discovery (`<OTA_HOSTNAME>.local`) AND the
// SSID broadcast by the SoftAP provisioning portal.
#define OTA_HOSTNAME  "f1-esp32"
// Password for both the ArduinoOTA upload protocol AND the SoftAP
// provisioning portal (must be >= 8 chars for WPA2; an empty value
// means open-network portal + no OTA password).
#define OTA_PASSWORD  "f1ota"
