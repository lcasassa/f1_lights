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
// Define SEGMENT_SCAN=<digit-bitmask> in the build env to identify which
// physical segment each of the 16 bits drives on your specific HT16K33
// backpack. The mask selects which of the 4 panel digits receive the probe
// pattern simultaneously, so wired-in-parallel LEDs (e.g. our Dig1 + Dig2
// pair sharing the same segment lines) all respond at once.
//
//   SEGMENT_SCAN=0x1   Dig1 only
//   SEGMENT_SCAN=0x2   Dig2 only
//   SEGMENT_SCAN=0x3   Dig1 AND Dig2     (recommended — default below)
//   SEGMENT_SCAN=0xF   all four digits
//
// Two modes, selected by SEGMENT_SCAN_MODE (default = binary search):
//
//   SEGMENT_SCAN_MODE=0  → sequential walk (lights bit 0..15, 700 ms each).
//   SEGMENT_SCAN_MODE=1  → interactive binary search over the serial
//                          monitor. Lights half of the still-candidate
//                          bits at once and asks "is your target lit?".
//                          Type y / n / a / r / q + Enter.
#ifdef SEGMENT_SCAN
#ifndef SEGMENT_SCAN_MODE
#define SEGMENT_SCAN_MODE 1   // 0 = sequential, 1 = interactive binary search
#endif

// Treat SEGMENT_SCAN as a bitmask of digits. Backward-compat: a value of 0
// (i.e. -DSEGMENT_SCAN=0) is interpreted as "Dig1 only" (bit 0 set).
static constexpr uint8_t kScanDigitMask = (uint8_t)((SEGMENT_SCAN) == 0 ? 0x1 : (SEGMENT_SCAN));

// Write the same 16-bit segment word to every digit selected by the mask.
static void scanWriteAllDigits(uint16_t segs) {
  for (uint8_t d = 0; d < 4; d++) {
    if (kScanDigitMask & (1u << d)) htWriteDigit(d, segs);
  }
}

// Comma-separated list of selected digits, e.g. "0,1".
static void printScanDigits() {
  bool first = true;
  for (uint8_t d = 0; d < 4; d++) {
    if (kScanDigitMask & (1u << d)) {
      Serial.printf("%s%u", first ? "" : ",", d);
      first = false;
    }
  }
}

static void runSegmentScanSequential() {
  Serial.print("SEGMENT_SCAN(seq): walking digits ");
  printScanDigits();
  Serial.println(", one bit at a time");
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
  Serial.print(
    "\nSEGMENT_SCAN(binary): two-stage interactive bit-finder\n"
    "  Search universe = digits ");
  printScanDigits();
  Serial.println(
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
    "  ~6 questions total to find one bit on one digit out of 4 digits.\n");

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
      for (uint8_t d = 0; d < 4 && taken < want; d++) {
        if (digCands & (1u << d)) { probeDigits |= (1u << d); taken++; }
      }

      htFillAll(0x00);
      for (uint8_t d = 0; d < 4; d++) {
        if (probeDigits & (1u << d)) htWriteDigit(d, allBits);
      }

      Serial.printf("\nQ%u (digit)  candidates [", question++);
      for (uint8_t d = 0, first = 1; d < 4; d++) {
        if (digCands & (1u << d)) {
          Serial.printf("%s%u", first ? "" : ",", d); first = 0;
        }
      }
      Serial.printf("]  lighting [");
      for (uint8_t d = 0, first = 1; d < 4; d++) {
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
        for (uint8_t d = 0; d < 4; d++) {
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

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-esp32 boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  setupLed();
  setupHt16k33();
#ifdef SEGMENT_SCAN
  runSegmentScan();             // never returns; for bit-mapping debug
#endif
  startupColorBlink();          // R / G / B self-test
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
    uint32_t now = millis();
    if (now - lastHue >= 250) {
      lastHue = now;
      hueIdx = (hueIdx + 1) % 6;
      const uint8_t *c = rainbow[hueIdx];
      htSetAllRgb(c[0], c[1], c[2]);
    }
  }

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("WiFi: dropped, reconnecting...");
    connectWifi();
    setupOta();
  }
}
