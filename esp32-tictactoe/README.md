# ESP32-C3 Super Mini — tic-tac-toe firmware

Stripped-down sibling of [`../esp32`](../esp32). The tic-tac-toe board has:

- One **RGB LED** soldered onto the HT16K33 14-seg backpack, on the digit at
  COM index 4, with `R=G2`, `G=D`, `B=DP`.
- **No front-panel buttons.** The F1 build's `BTN_A` / `BTN_B` boot-time
  arming + factory-reset hold sequence is therefore removed.
- **No 7-seg displays.** All `seg7::*` calls and the `segment_scan` debug
  tool are dropped from the build.
- **No buzzer by default** (`BUZZER_PIN=-1`); flip it back on with a build
  flag if/when one is wired up.

What's preserved verbatim from the F1 build:

- `ht16k33.{h,cpp}` low-level driver.
- WiFi station + WiFiManager captive-portal provisioning (`wifi_ota.cpp`).
- ArduinoOTA receiver on `<OTA_HOSTNAME>.local:3232`.
- GitHub rolling-release self-updater (compiled out with
  `-DDISABLE_GITHUB_OTA=1` on the lean env).

## Boot behaviour

1. Bring up the HT16K33 + the single RGB LED.
2. Quick R → G → B startup blink.
3. Try to associate with stored credentials for up to 15 s. On success,
   run the GitHub self-update check and arm ArduinoOTA. On failure,
   continue offline; `loop()` will keep retrying in the background.
4. Idle animation cycles the LED through R / G / B / off until real
   game logic is wired in.

## OTA assets

The self-updater fetches a separate pair of release assets so the F1 and
tic-tac-toe boards can't accidentally cross-flash each other:

- `version-tictactoe.txt`
- `firmware-tictactoe.bin`

Override at build time with `-DOTA_VERSION_ASSET=...` /
`-DOTA_FIRMWARE_ASSET=...` if you publish under different names.

## PlatformIO envs

```sh
pio run -e esp32-c3-tictactoe          # build + USB upload
pio run -e esp32-c3-tictactoe-ota      # WiFi push via ArduinoOTA
pio run -e esp32-c3-tictactoe-ota-fast # WiFi push, no GitHub self-updater
```

`wifi_credentials.h` follows the same convention as the F1 build — copy
`wifi_credentials.example.h`, edit, and let the on-device portal handle
the actual SSID/PSK.

