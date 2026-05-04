#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <ArduinoOTA.h>
#include <ESPmDNS.h>

// Optional GitHub self-update path. Pulls in HTTPClient + HTTPUpdate +
// WiFiClientSecure (mbedTLS), which adds ~100–150 KB to the firmware.
// Define DISABLE_GITHUB_OTA=1 in the build env (e.g. a "fast OTA" env that
// you only push over WiFi) to drop it and shrink the OTA payload.
#ifndef DISABLE_GITHUB_OTA
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#endif

#include "wifi_credentials.h"

// Build-time identity (overridden by CI: -DFIRMWARE_VERSION=\"<sha>\").
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
// GitHub repo "owner/name" hosting the firmware-latest release.
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif
// Asset names inside the rolling release.
#define OTA_RELEASE_TAG    "firmware-latest"
#define OTA_VERSION_URL    "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/version.txt"
#define OTA_FIRMWARE_URL   "https://github.com/" OTA_REPO "/releases/download/" OTA_RELEASE_TAG "/firmware.bin"

// I2C pins for HT16K33.
// Default GPIO 4 (SDA) / 5 (SCL) on the ESP32-C3 Super Mini — no strapping
// role, no shared on-board peripheral, only nominally conflicts with JTAG
// (we debug over USB CDC anyway). Override with -DI2C_SDA=<n> -DI2C_SCL=<n>
// from platformio.ini if your wiring differs.
#ifndef I2C_SDA
#define I2C_SDA 4
#endif
#ifndef I2C_SCL
#define I2C_SCL 5
#endif

// HT16K33 default I2C address (A0..A2 tied low).
static constexpr uint8_t HT16K33_ADDR = 0x70;

// HT16K33 commands
static constexpr uint8_t HT_SYS_ON        = 0x21; // oscillator on
static constexpr uint8_t HT_DISPLAY_ON    = 0x81; // display on, no blink
static constexpr uint8_t HT_BLINK_1HZ     = 0x85; // display on, blink at 1 Hz
static constexpr uint8_t HT_DIMMING_BASE  = 0xE0; // | brightness 0..15

// ── RGB LEDs piggy-backed on the 14-seg backpack ───────────────────────────
// Common-cathode (or common-anode — see RGB_ACTIVE_LOW) RGB LEDs are wired
// so each color leg is driven by one HT16K33 segment line on a given digit
// position. See kRgbLeds[] below for the live mapping.
//
// Segment-bit positions inside the 16-bit per-digit word
// (Adafruit Quad-Alphanumeric / LTP-305 style; M is on bit 11 on this
// specific backpack, not the textbook bit 13 — verified with SEGMENT_SCAN):
//   A=0, B=1, C=2, D=3, E=4, F=5, G1=6, G2=7, H=8, I=9, J=10, M=11, L=12,
//   bit 13 = unconnected, DP=14.
//
// Adjust the per-LED masks in kRgbLeds[] if your wiring maps colors to
// different segments (e.g. swap rMask/gMask to flip red and green).
static constexpr uint16_t SEG_A_MASK  = 1u << 0;   // 0x0001
static constexpr uint16_t SEG_B_MASK  = 1u << 1;   // 0x0002
static constexpr uint16_t SEG_C_MASK  = 1u << 2;   // 0x0004
static constexpr uint16_t SEG_D_MASK  = 1u << 3;   // 0x0008
static constexpr uint16_t SEG_E_MASK  = 1u << 4;   // 0x0010
static constexpr uint16_t SEG_F_MASK  = 1u << 5;   // 0x0020
static constexpr uint16_t SEG_G1_MASK = 1u << 6;   // 0x0040
static constexpr uint16_t SEG_G2_MASK = 1u << 7;   // 0x0080
static constexpr uint16_t SEG_H_MASK  = 1u << 8;   // 0x0100  (free again)
static constexpr uint16_t SEG_I_MASK  = 1u << 9;   // 0x0200
static constexpr uint16_t SEG_J_MASK  = 1u << 10;  // 0x0400
static constexpr uint16_t SEG_M_MASK  = 1u << 11;  // 0x0800  (M on this backpack)
// On this backpack the silkscreen-labelled "K" sits at bit 14. Confirmed
// with SEGMENT_SCAN (bit 14 lit the blue leg of LEDs #9/#10).
static constexpr uint16_t SEG_K_MASK  = 1u << 14;  // 0x4000
static constexpr uint16_t SEG_L_MASK  = 1u << 12;  // 0x1000
// On this backpack the silkscreen-labelled "DP" sits at bit 13, not the
// textbook bit 14 (same upper-half shift as M=11). Confirmed with
// SEGMENT_SCAN (bit 13 lit the blue leg of LEDs #7/#8).
static constexpr uint16_t SEG_DP_MASK = 1u << 13;  // 0x2000

// One RGB LED = which digit it lives on + per-color segment masks.
struct RgbLed {
  uint8_t  digit;
  uint16_t rMask;
  uint16_t gMask;
  uint16_t bMask;
};

// Order is documented above. RGB mapping for the F/I/J pair:
//   RED on F, GREEN on I, BLUE on J. Rotate if the wiring differs.
static constexpr RgbLed kRgbLeds[] = {
  {0, SEG_A_MASK, SEG_G1_MASK, SEG_B_MASK},  // LED #1 — Dig1, A/B/G1 (G&B flipped)
  {1, SEG_A_MASK, SEG_G1_MASK, SEG_B_MASK},  // LED #2 — Dig2, A/B/G1 (G&B flipped)
  {0, SEG_J_MASK, SEG_I_MASK,  SEG_F_MASK},  // LED #3 — Dig1, F/I/J (R&B flipped)
  {1, SEG_J_MASK, SEG_I_MASK,  SEG_F_MASK},  // LED #4 — Dig2, F/I/J (R&B flipped)
  {0, SEG_E_MASK, SEG_M_MASK,  SEG_L_MASK},  // LED #5 — Dig1, E/M/L
  {1, SEG_E_MASK, SEG_M_MASK,  SEG_L_MASK},  // LED #6 — Dig2, E/M/L
  {0, SEG_D_MASK, SEG_C_MASK,  SEG_DP_MASK}, // LED #7 — Dig1, C/DP/D (R&B then G&B flipped)
  {1, SEG_D_MASK, SEG_C_MASK,  SEG_DP_MASK}, // LED #8 — Dig2, C/DP/D (R&B then G&B flipped)
  {0, SEG_H_MASK, SEG_G2_MASK, SEG_K_MASK},  // LED #9  — Dig1, G2/K/H (R&B then G&B flipped)
  {1, SEG_H_MASK, SEG_G2_MASK, SEG_K_MASK},  // LED #10 — Dig2, G2/K/H (R&B then G&B flipped)
};
static constexpr uint8_t kNumRgbLeds = sizeof(kRgbLeds) / sizeof(kRgbLeds[0]);

// Set true if the LEDs are common-anode (segment line LOW = color ON).
// HT16K33 sinks current → for common-cathode RGB LEDs (anode legs into the
// segment lines via current-limit resistors) leave this false.
#ifndef RGB_ACTIVE_LOW
#define RGB_ACTIVE_LOW 0
#endif

// True while an OTA upload is in flight; suppresses the normal heartbeat blink
// in loop() so it doesn't fight the progress-driven toggles.
static volatile bool otaInProgress = false;

static inline void htCmd(uint8_t c) {
  Wire.beginTransmission(HT16K33_ADDR);
  Wire.write(c);
  Wire.endTransmission();
}

// Fill all 16 display-RAM bytes (= all 128 LEDs) with the given byte.
static void htFillAll(uint8_t v) {
  Wire.beginTransmission(HT16K33_ADDR);
  Wire.write(0x00); // start at RAM address 0
  for (uint8_t i = 0; i < 16; i++) Wire.write(v);
  Wire.endTransmission();
}

// Write the 16-bit segment word for a single digit (RAM addresses 2*digit
// and 2*digit+1, low byte first).
static void htWriteDigit(uint8_t digit, uint16_t segs) {
  Wire.beginTransmission(HT16K33_ADDR);
  Wire.write((uint8_t)(digit * 2));
  Wire.write((uint8_t)(segs & 0xFF));
  Wire.write((uint8_t)((segs >> 8) & 0xFF));
  Wire.endTransmission();
}

// Drive every RGB LED to the same color. r/g/b are booleans (no per-color
// PWM — the HT16K33 has only a global brightness register).
//
// For each digit we accumulate the segment masks of *all* RGB LEDs that
// live on it, then apply RGB_ACTIVE_LOW inversion only to that union of
// RGB-controlled bits.
static void htSetAllRgb(bool r, bool g, bool b) {
  // Per-digit accumulators (panel has 4 digits worth of RAM).
  uint16_t rgbBits[4]  = {0, 0, 0, 0};
  uint16_t rgbMask[4]  = {0, 0, 0, 0};   // union of all RGB-controlled bits
  for (uint8_t i = 0; i < kNumRgbLeds; i++) {
    const RgbLed &led = kRgbLeds[i];
    uint8_t d = led.digit;
    rgbMask[d] |= led.rMask | led.gMask | led.bMask;
    if (r) rgbBits[d] |= led.rMask;
    if (g) rgbBits[d] |= led.gMask;
    if (b) rgbBits[d] |= led.bMask;
  }
  for (uint8_t d = 0; d < 4; d++) {
    if (rgbMask[d] == 0) continue;
    uint16_t segs = rgbBits[d];
#if RGB_ACTIVE_LOW
    segs = (~segs) & rgbMask[d];   // invert only RGB-controlled bits
#endif
    htWriteDigit(d, segs);
  }
}

static void setupHt16k33() {
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // HT16K33 supports up to 400 kHz

  htCmd(HT_SYS_ON);
  htCmd(HT_DIMMING_BASE | 2);
  htCmd(HT_DISPLAY_ON);        // display on, no hardware blink
  htFillAll(0x00);             // start with everything off
  htSetAllRgb(true, true, true); // all RGB LEDs white as a power-on indicator
  Serial.println("HT16K33: setup done");
}

// Startup self-test: blink each color channel three times in sequence
// (red → green → blue) on every RGB LED.
static void startupColorBlink() {
  struct Phase { bool r, g, b; const char *name; };
  static const Phase phases[3] = {
    {true,  false, false, "RED"},
    {false, true,  false, "GREEN"},
    {false, false, true,  "BLUE"},
  };
  constexpr uint8_t kBlinks    = 3;
  constexpr uint16_t kOnMs     = 250;
  constexpr uint16_t kOffMs    = 250;
  constexpr uint16_t kGapMs    = 300;  // pause between colors

  for (const auto &p : phases) {
    Serial.printf("startup: blinking %s\n", p.name);
    for (uint8_t i = 0; i < kBlinks; i++) {
      htSetAllRgb(p.r, p.g, p.b);
      delay(kOnMs);
      htSetAllRgb(false, false, false);
      delay(kOffMs);
    }
    delay(kGapMs);
  }
}

// Onboard LED on GPIO8 (active-low). Free again now that I2C lives on
// GPIO 4/5. Use for cheap visual debugging from the bare board.
#ifndef ONBOARD_LED_PIN
#define ONBOARD_LED_PIN 8
#endif
static inline void setupLed() {
  pinMode(ONBOARD_LED_PIN, OUTPUT);
  digitalWrite(ONBOARD_LED_PIN, HIGH);  // off (active-low)
}
static inline void setLed(bool on) {
  digitalWrite(ONBOARD_LED_PIN, on ? LOW : HIGH);
}

// ── Segment-bit scanner (debug aid) ────────────────────────────────────────
// Define SEGMENT_SCAN=1 in the build env to enable the bit-mapping scanner.
// It always probes ALL 8 HT16K33 COM lines (digits 0..7), so any wired-in-
// parallel LED responds regardless of which digit it lives on.
//
// Two modes, selected by SEGMENT_SCAN_MODE (default = binary search):
//
//   SEGMENT_SCAN_MODE=0  → sequential walk (lights bit 0..15, 700 ms each).
//   SEGMENT_SCAN_MODE=1  → interactive binary search over the serial
//                          monitor. Lights half of the still-candidate
//                          bits at once and asks "is your target lit?".
//                          Type y / n / a / r / q + Enter.
#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)
#ifndef SEGMENT_SCAN_MODE
#define SEGMENT_SCAN_MODE 1   // 0 = sequential, 1 = interactive binary search
#endif

// HT16K33 has 8 COM lines → 8 addressable digits of 16 bits each.
static constexpr uint8_t kScanNumDigits = 8;
static constexpr uint8_t kScanDigitMask = 0xFF;   // all 8 digits in play

// Write the same 16-bit segment word to every digit.
static void scanWriteAllDigits(uint16_t segs) {
  for (uint8_t d = 0; d < kScanNumDigits; d++) {
    htWriteDigit(d, segs);
  }
}

static void runSegmentScanSequential() {
  Serial.println("SEGMENT_SCAN(seq): walking all 8 digits, one bit at a time");
  while (true) {
    for (uint8_t bit = 0; bit < 16; bit++) {
      uint16_t mask = (uint16_t)(1u << bit);
      htFillAll(0x00);
      scanWriteAllDigits(mask);
      Serial.printf("  bit %2u → 0x%04X\n", bit, mask);
      delay(700);
    }
  }
}

// Block until the user sends a single non-whitespace character; returns it
// lower-cased. Echoes input so the monitor shows what you typed.
static char readSerialChar() {
  for (;;) {
    while (!Serial.available()) delay(5);
    int c = Serial.read();
    if (c < 0) continue;
    if (c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
    char ch = (char)c;
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
    Serial.printf(" -> '%c'\n", ch);
    return ch;
  }
}

// List the bit indices set in `mask` as "0,3,7,12".
static void printBits(uint16_t mask) {
  bool first = true;
  for (uint8_t b = 0; b < 16; b++) {
    if (mask & (1u << b)) {
      Serial.printf("%s%u", first ? "" : ",", b);
      first = false;
    }
  }
}

// Pick a sub-mask containing roughly half of the bits currently set in
// `candidates`, in low-to-high order. Used as the "yes" half during the
// binary search.
static uint16_t lowerHalfOf(uint16_t candidates) {
  uint8_t total = __builtin_popcount(candidates);
  uint8_t want  = total / 2;
  if (want == 0) want = 1;
  uint16_t out = 0;
  uint8_t taken = 0;
  for (uint8_t b = 0; b < 16 && taken < want; b++) {
    uint16_t m = (uint16_t)(1u << b);
    if (candidates & m) {
      out |= m;
      taken++;
    }
  }
  return out;
}

static void runSegmentScanBinary() {
  Serial.println(
    "\nSEGMENT_SCAN(binary): two-stage interactive bit-finder\n"
    "  Search universe = all 8 HT16K33 COM lines (digits 0..7)\n"
    "\n"
    "  Stage 1: narrow down which DIGIT your target lives on by lighting\n"
    "           ALL 16 segments on half of the candidate digits.\n"
    "  Stage 2: narrow down which BIT inside that digit.\n"
    "  Answers:\n"
    "    y = my target IS lit in this set\n"
    "    n = my target is NOT lit\n"
    "    a = light EVERYTHING on every candidate digit for 1 s\n"
    "    r = restart from stage 1\n"
    "    q = quit (returns to normal firmware)\n"
    "  ~7 questions total to find one bit on one digit out of 8 digits.\n");

  while (true) {
    // ── Stage 1: digit search ────────────────────────────────────────────
    uint8_t  digCands = kScanDigitMask;     // candidate digits as bitmask
    uint8_t  question = 1;
    uint16_t allBits  = 0xFFFF;             // light every segment for stage 1

    while (__builtin_popcount(digCands) > 1) {
      // Pick the lower half of the candidate digits.
      uint8_t total = __builtin_popcount(digCands);
      uint8_t want  = total / 2; if (want == 0) want = 1;
      uint8_t probeDigits = 0, taken = 0;
      for (uint8_t d = 0; d < kScanNumDigits && taken < want; d++) {
        if (digCands & (1u << d)) { probeDigits |= (1u << d); taken++; }
      }

      htFillAll(0x00);
      for (uint8_t d = 0; d < kScanNumDigits; d++) {
        if (probeDigits & (1u << d)) htWriteDigit(d, allBits);
      }

      Serial.printf("\nQ%u (digit)  candidates [", question++);
      for (uint8_t d = 0, first = 1; d < kScanNumDigits; d++) {
        if (digCands & (1u << d)) {
          Serial.printf("%s%u", first ? "" : ",", d); first = 0;
        }
      }
      Serial.printf("]  lighting [");
      for (uint8_t d = 0, first = 1; d < kScanNumDigits; d++) {
        if (probeDigits & (1u << d)) {
          Serial.printf("%s%u", first ? "" : ",", d); first = 0;
        }
      }
      Serial.printf("]  → is your target lit? (y/n/a/r/q)");
      char ans = readSerialChar();
      if (ans == 'r') { digCands = kScanDigitMask; question = 1; continue; }
      if (ans == 'q') { htFillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
      if (ans == 'a') {
        Serial.println("  ALL ON 1 s ...");
        for (uint8_t d = 0; d < kScanNumDigits; d++) {
          if (kScanDigitMask & (1u << d)) htWriteDigit(d, 0xFFFF);
        }
        delay(1000);
        question--;
        continue;
      }
      if      (ans == 'y') digCands &=  probeDigits;
      else if (ans == 'n') digCands &= ~probeDigits;
      else { Serial.println("  ?? expected y/n/a/r/q"); question--; }
    }

    if (digCands == 0) {
      Serial.println("\nSEGMENT_SCAN: no digit candidate left. Restart.");
      continue;
    }

    uint8_t digit = (uint8_t)__builtin_ctz(digCands);
    Serial.printf("\n→ digit narrowed to %u, now searching its 16 bits\n", digit);

    // ── Stage 2: bit search on the chosen digit ──────────────────────────
    uint16_t cands = 0xFFFF;
    while (__builtin_popcount(cands) > 1) {
      uint16_t probe = lowerHalfOf(cands);
      htFillAll(0x00);
      htWriteDigit(digit, probe);
      Serial.printf("\nQ%u (bit)  candidates [", question++);
      printBits(cands);
      Serial.printf("]  lighting [");
      printBits(probe);
      Serial.printf("]  → is your target lit? (y/n/a/r/q)");
      char ans = readSerialChar();
      if (ans == 'r') { cands = 0xFFFF; continue; }   // restart bit search only
      if (ans == 'q') { htFillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
      if (ans == 'a') {
        Serial.println("  ALL ON 1 s ...");
        htWriteDigit(digit, 0xFFFF);
        delay(1000);
        htWriteDigit(digit, probe);
        question--;
        continue;
      }
      if      (ans == 'y') cands &=  probe;
      else if (ans == 'n') cands &= ~probe;
      else { Serial.println("  ?? expected y/n/a/r/q"); question--; }
    }

    if (cands == 0) {
      Serial.println("\nSEGMENT_SCAN: no bit candidate left. Restart.");
      continue;
    }

    uint8_t bit = (uint8_t)__builtin_ctz(cands);
    htFillAll(0x00);
    htWriteDigit(digit, cands);
    Serial.printf("\n*** FOUND: digit %u, bit %u  (mask 0x%04X) ***\n",
                  digit, bit, cands);
    Serial.println("    Update the matching SEG_*_MASK / kRgbLeds[] in main.cpp.");
    Serial.println("    Press 'r' to find another segment, 'q' to quit.");
    char ans = readSerialChar();
    if (ans == 'q') { htFillAll(0x00); Serial.println("quit"); delay(200); ESP.restart(); }
    // anything else → loop back to stage 1
  }
}

static void runSegmentScan() {
#if SEGMENT_SCAN_MODE == 0
  runSegmentScanSequential();
#else
  runSegmentScanBinary();
#endif
}
#endif

static void connectWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setHostname(OTA_HOSTNAME);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("WiFi: connecting to '%s'", WIFI_SSID);
  uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print('.');
    delay(200);
    if (millis() - start > 30000) {
      Serial.println("\nWiFi: timeout, restarting...");
      ESP.restart();
    }
  }
  Serial.printf("\nWiFi: connected, IP=%s\n", WiFi.localIP().toString().c_str());
}

static void setupOta() {
  ArduinoOTA.setHostname(OTA_HOSTNAME);
  if (strlen(OTA_PASSWORD) > 0) {
    ArduinoOTA.setPassword(OTA_PASSWORD);
  }

  ArduinoOTA
    .onStart([]() {
      otaInProgress = true;
      Serial.printf("OTA: start (%s)\n",
                    ArduinoOTA.getCommand() == U_FLASH ? "sketch" : "fs");
      htFillAll(0xFF);                  // begin with all LEDs on
    })
    .onEnd([]() {
      Serial.println("\nOTA: done");
      htFillAll(0xFF);                  // leave all on before reboot
      otaInProgress = false;
    })
    .onProgress([](unsigned int p, unsigned int t) {
      Serial.printf("OTA: %u%%\r", (p * 100) / t);
      // Toggle every 1% of progress.
      static int lastBucket = -1;
      static bool on = true;
      int bucket = (int)((uint64_t)p * 100 / t);  // 0..100 = every 1%
      if (bucket != lastBucket) {
        lastBucket = bucket;
        on = !on;
        htFillAll(on ? 0xFF : 0x00);
      }
    })
    .onError([](ota_error_t e) {
      otaInProgress = false;
      Serial.printf("OTA error[%u]: ", e);
      switch (e) {
        case OTA_AUTH_ERROR:    Serial.println("auth");    break;
        case OTA_BEGIN_ERROR:   Serial.println("begin");   break;
        case OTA_CONNECT_ERROR: Serial.println("connect"); break;
        case OTA_RECEIVE_ERROR: Serial.println("receive"); break;
        case OTA_END_ERROR:     Serial.println("end");     break;
      }
    });

  ArduinoOTA.begin();
  Serial.printf("OTA: ready at %s.local (%s)\n",
                OTA_HOSTNAME, WiFi.localIP().toString().c_str());
}

// ── GitHub self-update on boot ──────────────────────────────────────────────
//
// Fetches version.txt from the rolling 'firmware-latest' release; if the
// content differs from FIRMWARE_VERSION, downloads firmware.bin and applies
// it via HTTPUpdate (which reboots on success).
//
// Compiled out when DISABLE_GITHUB_OTA is defined (saves ~100–150 KB and
// makes WiFi OTA noticeably faster during dev iteration).

#ifndef DISABLE_GITHUB_OTA
static String fetchLatestVersion() {
  WiFiClientSecure client;
  client.setInsecure();           // skip CA validation (good enough for LAN OTA)
  client.setTimeout(10);

  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("f1-esp32-ota");
  if (!http.begin(client, OTA_VERSION_URL)) {
    Serial.println("OTA-check: http.begin failed");
    return String();
  }
  int code = http.GET();
  String body;
  if (code == HTTP_CODE_OK) {
    body = http.getString();
    body.trim();
  } else {
    Serial.printf("OTA-check: HTTP %d\n", code);
  }
  http.end();
  return body;
}

static void checkAndUpdateFromGithub() {
  Serial.printf("OTA-check: running version '%s'\n", FIRMWARE_VERSION);
  String latest = fetchLatestVersion();
  if (latest.isEmpty()) {
    Serial.println("OTA-check: could not read latest version, skipping");
    return;
  }
  Serial.printf("OTA-check: latest = '%s'\n", latest.c_str());
  if (latest == FIRMWARE_VERSION) {
    Serial.println("OTA-check: already up to date");
    return;
  }

  Serial.println("OTA-check: new version available, downloading...");
  htFillAll(0xFF);                              // signal: update starting

  WiFiClientSecure client;
  client.setInsecure();
  client.setTimeout(20);

  httpUpdate.rebootOnUpdate(true);
  httpUpdate.onStart([]() {
    Serial.println("HTTPUpdate: start");
  });
  httpUpdate.onProgress([](int cur, int total) {
    static int lastBucket = -1;
    static bool on = true;
    if (total <= 0) return;
    int bucket = (int)((int64_t)cur * 100 / total);  // every 1%
    if (bucket != lastBucket) {
      lastBucket = bucket;
      on = !on;
      htFillAll(on ? 0xFF : 0x00);
      if (bucket % 10 == 0) Serial.printf("HTTPUpdate: %d%%\n", bucket);
    }
  });
  httpUpdate.onError([](int err) {
    Serial.printf("HTTPUpdate: error %d: %s\n",
                  err, httpUpdate.getLastErrorString().c_str());
  });

  t_httpUpdate_return res = httpUpdate.update(client, OTA_FIRMWARE_URL, FIRMWARE_VERSION);
  switch (res) {
    case HTTP_UPDATE_FAILED:
      Serial.printf("HTTPUpdate: failed (%d): %s\n",
                    httpUpdate.getLastError(),
                    httpUpdate.getLastErrorString().c_str());
      break;
    case HTTP_UPDATE_NO_UPDATES:
      Serial.println("HTTPUpdate: no updates");
      break;
    case HTTP_UPDATE_OK:
      Serial.println("HTTPUpdate: ok (rebooting)");
      break;
  }
}
#else  // DISABLE_GITHUB_OTA
static inline void checkAndUpdateFromGithub() {
  Serial.println("OTA-check: GitHub self-update disabled at build time");
}
#endif

// Forward declarations for the 5-digit 7-seg helpers (definitions live in
// the seg7 namespace at the bottom of the file).
namespace seg7 {
  static void writeText(const char *s);
  static void printFloat(float value);
  static void clear();
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-esp32 boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  setupLed();
  setupHt16k33();
#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)
  runSegmentScan();             // never returns; for bit-mapping debug
#endif
  startupColorBlink();          // R / G / B self-test
  // 7-seg self-test, three frames so every segment + every DP is exercised:
  //   1) "8.8.8.8.8."  → every segment + every DP on (all 5 cells lit full)
  //   2) "2.2.222"     → digit '2' on every cell, DP on the leftmost 2
  //   3) "555.5.5."    → digit '5' on every cell, DP on the rightmost 3
  // Frames 2+3 cover the same 5 DPs that frame 1 lit, split 2 + 3.
  seg7::writeText("8.8.8.8.8.");
  delay(700);
  seg7::writeText("2.2222.");
  delay(700);
  seg7::writeText("55.5.5.5");
  delay(700);
  connectWifi();
  checkAndUpdateFromGithub();   // may reboot into new firmware
  setupOta();
}

void loop() {
  ArduinoOTA.handle();

  if (!otaInProgress) {
    // Rainbow sweep on every RGB LED (R→Y→G→C→B→M→…), 250 ms/step.
    static const uint8_t rainbow[6][3] = {
      {1, 0, 0}, // red
      {1, 1, 0}, // yellow
      {0, 1, 0}, // green
      {0, 1, 1}, // cyan
      {0, 0, 1}, // blue
      {1, 0, 1}, // magenta
    };
    static uint32_t lastHue = 0;
    static uint8_t hueIdx = 0;
    static float segCounter = 0;
    uint32_t now = millis();
    if (now - lastHue >= 250) {
      lastHue = now;
      hueIdx = (hueIdx + 1) % 6;
      const uint8_t *c = rainbow[hueIdx];
      htSetAllRgb(c[0], c[1], c[2]);
      // Tick the 7-seg counter in lockstep with the rainbow: each new
      // color shows the next number on the 5-digit panel.
      seg7::printFloat(segCounter);
      // Random increment in [minInc, 0.1]. minInc grows with the integer
      // part so the smallest step is always at least one visible LSB on
      // the 5-digit panel (e.g. value=12 → 3 fractional digits → min 0.001).
      float minInc;
      uint32_t v = (uint32_t)segCounter;
      if      (v >= 10000) minInc = 1.0f;     // no fractional cells
      else if (v >= 1000)  minInc = 0.1f;     // 1 fractional digit
      else if (v >= 100)   minInc = 0.01f;    // 2 fractional digits
      else if (v >= 10)    minInc = 0.001f;   // 3 fractional digits
      else                 minInc = 0.0001f;  // 4 fractional digits
      const float maxInc = 0.1f;
      if (minInc > maxInc) minInc = maxInc;
      float t = (float)random(0, 10001) / 10000.0f;          // 0..1
      segCounter += minInc + t * (maxInc - minInc);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: dropped, reconnecting...");
    connectWifi();
    setupOta();
  }
}

// ── 5-digit 7-segment display ──────────────────────────────────────────────
// Wired to the same HT16K33 chip, sharing ROW lines with the 14-seg
// backpack but using its own COM lines. Bit map confirmed with the segscan
// (see chat log): every 7-seg segment found on a single ROW, regardless of
// which COM the character lives on.
//
//   segment  ROW (bit)
//     A   →  ROW 5   (also F on the 14-seg — different COM, no conflict)
//     B   →  ROW 9   (also I on the 14-seg)
//     C   →  ROW 0   (also A on the 14-seg)
//     D   →  ROW10   (also J on the 14-seg)
//     E   →  ROW 1   (also B on the 14-seg)
//     F   →  ROW 8   (also H on the 14-seg)
//     G   →  ROW 6   (also G1 on the 14-seg)
//     DP  →  ROW 2   (also C on the 14-seg)
namespace seg7 {

static constexpr uint16_t A  = 1u << 5;
static constexpr uint16_t B  = 1u << 9;
static constexpr uint16_t C  = 1u << 0;
static constexpr uint16_t D  = 1u << 10;
static constexpr uint16_t E  = 1u << 1;
static constexpr uint16_t F  = 1u << 8;
static constexpr uint16_t G  = 1u << 6;
static constexpr uint16_t DP = 1u << 2;

// Left-to-right COM index for each character cell. Confirmed with the
// segscan by lighting segment A (bit 5) on each candidate COM and noting
// which physical digit lit up:
//   cell 0 (far left)  → COM 7
//   cell 1             → COM 6
//   cell 2             → COM 5
//   cell 3             → COM 3   (note: COM 4 is skipped on this wiring)
//   cell 4 (far right) → COM 2
static constexpr uint8_t kComs[] = {7, 6, 5, 3, 2};
static constexpr uint8_t kNumDigits = sizeof(kComs) / sizeof(kComs[0]);

// Standard 7-seg glyphs. Returns 0 for "blank" or unknown chars.
static uint16_t encode(char c) {
  switch (c) {
    case '0': return A | B | C | D | E | F;
    case '1': return B | C;
    case '2': return A | B | G | E | D;
    case '3': return A | B | G | C | D;
    case '4': return F | G | B | C;
    case '5': return A | F | G | C | D;
    case '6': return A | F | G | E | C | D;
    case '7': return A | B | C;
    case '8': return A | B | C | D | E | F | G;
    case '9': return A | B | C | D | F | G;
    case '-': return G;
    case '_': return D;
    case ' ': return 0;
    case 'A': case 'a': return A | B | C | E | F | G;
    case 'b':          return C | D | E | F | G;
    case 'C': case 'c': return A | D | E | F;
    case 'd':          return B | C | D | E | G;
    case 'E': case 'e': return A | D | E | F | G;
    case 'F': case 'f': return A | E | F | G;
    case 'H': case 'h': return B | C | E | F | G;
    case 'L': case 'l': return D | E | F;
    case 'P': case 'p': return A | B | E | F | G;
    case 'r':          return E | G;
    case 'o':          return C | D | E | G;
    case 'S': case 's': return A | F | G | C | D;   // same shape as '5'
    default:           return 0;
  }
}

// Render a left-padded string across the panel. `s` may contain '.'
// characters which are folded into the *previous* glyph's DP segment
// (so "12.34" fits in 4 cells, not 5). Extra characters are truncated;
// missing characters are blank-padded on the LEFT.
static void writeText(const char *s) {
  // First pass: pack into a (glyph, dp) buffer of size kNumDigits, right-
  // aligned (so "12" → "   12", "12.34" → " 12.34").
  uint16_t glyphs[kNumDigits];
  bool     dps[kNumDigits];
  for (uint8_t i = 0; i < kNumDigits; i++) { glyphs[i] = 0; dps[i] = false; }

  // Walk source right-to-left, filling buffer from the right.
  size_t len = strlen(s);
  int8_t out = (int8_t)kNumDigits - 1;
  for (int i = (int)len - 1; i >= 0 && out >= 0; i--) {
    char c = s[i];
    if (c == '.') {
      // Attach DP to the next non-dot char to the left, written into the
      // current output cell (we don't consume an output slot for '.').
      if (i == 0) break;
      // Find the next non-dot char to the left.
      int j = i - 1;
      while (j >= 0 && s[j] == '.') j--;
      if (j < 0) break;
      glyphs[out] = encode(s[j]);
      dps[out]    = true;
      out--;
      i = j;       // outer loop will i-- past this char
    } else {
      glyphs[out] = encode(c);
      out--;
    }
  }

  // Second pass: write each cell to its COM, with DP folded in.
  for (uint8_t i = 0; i < kNumDigits; i++) {
    uint16_t segs = glyphs[i] | (dps[i] ? DP : 0);
    htWriteDigit(kComs[i], segs);
  }
}

// Render a float with as many fractional digits as will fit. Negative
// values consume one cell for '-'. If the integer part alone overflows
// the panel width, displays "-----" (overflow).
//
//   value      → 5-cell render
//   3.14159    → "3.142" (rounded)
//   -3.14159   → "-3.14"
//   1234.5     → "1234.5" (uses all 5 cells, no fractional truncation)
//   12345.6    → "12345"  (no room for '.' / fractional, integer fits)
//   123456.7   → "-----"  (overflow)
static void printFloat(float value) {
  // Sign.
  bool neg = value < 0;
  if (neg) value = -value;

  // Integer-part digit count (at least 1, for "0").
  uint32_t intPart = (uint32_t)value;
  uint8_t  intDigits = 1;
  for (uint32_t t = intPart; t >= 10; t /= 10) intDigits++;

  // Cells available for the integer part.
  uint8_t budget = kNumDigits - (neg ? 1 : 0);
  if (intDigits > budget) {
    // Overflow: print "-" filled across the panel.
    char buf[kNumDigits + 1];
    for (uint8_t i = 0; i < kNumDigits; i++) buf[i] = '-';
    buf[kNumDigits] = '\0';
    writeText(buf);
    return;
  }

  // Fractional cells = leftover after sign + integer digits. The '.' is
  // free (folded into the previous cell's DP).
  uint8_t fracDigits = budget - intDigits;

  // Round to `fracDigits` decimals.
  float scale = 1.0f;
  for (uint8_t i = 0; i < fracDigits; i++) scale *= 10.0f;
  uint32_t scaled = (uint32_t)(value * scale + 0.5f);

  // Re-extract integer & fractional parts after rounding (rounding may
  // have bumped intPart, which could in turn overflow — re-check).
  uint32_t newIntPart  = scaled / (uint32_t)scale;
  uint32_t newFracPart = scaled % (uint32_t)scale;
  uint8_t  newIntDigits = 1;
  for (uint32_t t = newIntPart; t >= 10; t /= 10) newIntDigits++;
  if (newIntDigits > budget) {
    char buf[kNumDigits + 1];
    for (uint8_t i = 0; i < kNumDigits; i++) buf[i] = '-';
    buf[kNumDigits] = '\0';
    writeText(buf);
    return;
  }

  // Format into a string. Worst case: '-' + intDigits + '.' + fracDigits.
  char buf[16];
  int  pos = 0;
  if (neg) buf[pos++] = '-';
  // Integer part.
  char tmp[12];
  int  tlen = 0;
  if (newIntPart == 0) tmp[tlen++] = '0';
  while (newIntPart > 0) { tmp[tlen++] = (char)('0' + newIntPart % 10); newIntPart /= 10; }
  while (tlen > 0) buf[pos++] = tmp[--tlen];
  // Fractional part.
  if (fracDigits > 0) {
    buf[pos++] = '.';
    // Zero-pad on the left of the fractional digits.
    char ftmp[12];
    int  flen = 0;
    for (uint8_t i = 0; i < fracDigits; i++) {
      ftmp[flen++] = (char)('0' + newFracPart % 10);
      newFracPart /= 10;
    }
    while (flen > 0) buf[pos++] = ftmp[--flen];
  }
  buf[pos] = '\0';
  writeText(buf);
}

// Clear all 7-seg cells (leaves the 14-seg / RGB LEDs untouched).
static void clear() {
  for (uint8_t i = 0; i < kNumDigits; i++) htWriteDigit(kComs[i], 0);
}

}  // namespace seg7

