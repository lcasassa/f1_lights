// WiFi credentials for the ESP32-C3 OTA build.
//
// Copy this file to `wifi_credentials.h` (same directory) and fill in
// your network's SSID and password. The real file is git-ignored.
#pragma once

#define WIFI_SSID     "your-ssid-here"
#define WIFI_PASSWORD "your-password-here"

// mDNS hostname used for OTA discovery: <OTA_HOSTNAME>.local
#define OTA_HOSTNAME  "f1-esp32"

// Optional OTA password (set to "" to disable). Must match Makefile/pio config.
#define OTA_PASSWORD  "f1ota"

