// WiFi station setup, ArduinoOTA receiver, and the optional GitHub
// rolling-release self-updater. Compile out the GitHub path with
// -DDISABLE_GITHUB_OTA=1 to save ~100–150 KB on the firmware image.
#pragma once

namespace wifi_ota {

// True while an OTA upload (Arduino or HTTP) is in flight; the main loop
// uses this to suppress its own RGB animation.
extern volatile bool inProgress;

// Connect to the configured WiFi network. Reboots after a 30 s timeout.
void connectWifi();

// Start the ArduinoOTA receiver (mDNS hostname + password from
// wifi_credentials.h). Call after WiFi is up.
void setupArduinoOta();

// One-shot: fetch version.txt from the rolling release and, if the SHA
// differs from FIRMWARE_VERSION, download + apply firmware.bin (which
// reboots on success). Compiled to a stub when DISABLE_GITHUB_OTA=1.
void checkAndUpdateFromGithub();

// Pump the ArduinoOTA receiver. Call from loop().
void handleOta();

}  // namespace wifi_ota

