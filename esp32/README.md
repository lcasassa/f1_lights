# ESP32-C3 Super Mini firmware

The sketch does three things:

1. Drives an HT16K33 backpack on I2C (SDA=GPIO8, SCL=GPIO9), blinking all LEDs at 2 Hz.
2. Joins WiFi, exposes ArduinoOTA on `f1-esp32.local:3232` (password `f1ota`) for fast local pushes.
3. On every boot, checks GitHub for a newer firmware build and self-updates over HTTPS if one is available.

## GitHub-pull OTA

- `.github/workflows/esp32-firmware.yml` runs on each push to `main`, builds with
  PlatformIO, and uploads `firmware.bin` + `version.txt` to the rolling release
  tag `firmware-latest`.
- On boot the device fetches
  `https://github.com/lcasassa/f1_lights/releases/download/firmware-latest/version.txt`
  and compares it to the `FIRMWARE_VERSION` baked into the running image.
- If they differ it downloads `firmware.bin` from the same release, applies it
  via `httpUpdate`, then reboots into the new firmware.

## Required repo secrets

Add under **Settings → Secrets and variables → Actions**:

| Secret          | Required | Default     |
|-----------------|----------|-------------|
| `WIFI_SSID`     | yes      | —           |
| `WIFI_PASSWORD` | yes      | —           |
| `OTA_HOSTNAME`  | no       | `f1-esp32`  |
| `OTA_PASSWORD`  | no       | `f1ota`     |

CI renders these into a temporary `esp32/wifi_credentials.h` before building,
so the published `firmware.bin` contains your WiFi credentials. The release is
public, so the binary is too — treat accordingly.

## Local development

```sh
make upload-esp32        # USB flash (hold BOOT + tap RST first time)
make upload-esp32-ota    # Push over WiFi via mDNS
make monitor-esp32       # Serial logs
```

Local builds default `FIRMWARE_VERSION` to `"dev"`, which never matches a CI
SHA, so a `dev` device boots online and immediately pulls down the latest CI
build.
