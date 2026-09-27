// f1_lights screen — ESP32-C3 firmware entry point.
//
// Hardware: 16 × 32 WS2812 matrix made of two 8 × 32 vertical panels
// (data lines on PANEL_A_PIN / PANEL_B_PIN, defaults GPIO 6 / 7) plus
// 4 momentary push-buttons (LEFT, RIGHT, ROTATE, DROP) on GPIO
// 0 / 1 / 3 / 10. No HT16K33, no buzzer, no I2C — see screen_panel.h
// + peripherals.h for the pin map.
//
// Boot flow (mirrors esp32-tictactoe/main.cpp):
//   1. Bring up panel + buttons + onboard LED.
//   2. Quick startup blink across the matrix.
//   3. If LEFT+RIGHT held at boot, run the WiFi-arm path: blocking
//      associate + GitHub self-update + ArduinoOTA. Otherwise kick off
//      a non-blocking associate that loop() finishes when WL_CONNECTED
//      arrives.
//   4. loop(): handle OTA, run the Tetris game.

#include <Arduino.h>
#include <WiFi.h>

#include "peripherals.h"
#include "screen_panel.h"
#include "wifi_ota.h"

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#ifndef OTA_REPO
#define OTA_REPO "lcasassa/f1_lights"
#endif

static constexpr uint32_t kBootConnectWaitMs = 15000;
static bool g_otaReady = false;

// ─── Tetris game state (ported from screen/main.cpp) ────────────────────────
namespace {

using screen_panel::MATRIX_W;
using screen_panel::MATRIX_H;
using screen_panel::pack;

constexpr uint8_t  PLAY_W  = 12;
constexpr uint8_t  PLAY_H  = MATRIX_H;     // full 32 rows
constexpr uint8_t  BAR_X   = 15;           // far-right vertical progress bar
constexpr uint8_t  BAR_PIX = PLAY_H;
constexpr uint8_t  NUM_PIECES = 7;

constexpr unsigned long FALL_MS      = 300;
constexpr unsigned long FAST_FALL_MS = 30;
constexpr unsigned long DEBOUNCE_MS  = 150;

// Piece colours (2-bit per channel packed format).
const uint8_t PIECE_COL[NUM_PIECES] = {
  pack(0,1,1),  // I  Cyan
  pack(1,1,0),  // O  Yellow
  pack(1,0,1),  // T  Purple
  pack(0,1,0),  // S  Green
  pack(1,0,0),  // Z  Red
  pack(1,1,0),  // L  Orange-ish (yellow w/ 2bit colour space)
  pack(0,0,1),  // J  Blue
};
const uint8_t PIECE_COL_DIM[NUM_PIECES] = {
  pack(0,1,1), pack(1,1,0), pack(1,0,1), pack(0,1,0),
  pack(1,0,0), pack(1,1,0), pack(0,0,1),
};

// Each piece: 4 blocks as {dx, dy} offsets in spawn rotation.
const int8_t PIECE_BLOCKS[NUM_PIECES][4][2] = {
  {{0,0},{1,0},{2,0},{3,0}},  // I
  {{0,0},{1,0},{0,1},{1,1}},  // O
  {{0,0},{1,0},{2,0},{1,1}},  // T
  {{1,0},{2,0},{0,1},{1,1}},  // S
  {{0,0},{1,0},{1,1},{2,1}},  // Z
  {{0,0},{0,1},{0,2},{1,2}},  // L
  {{1,0},{1,1},{1,2},{0,2}},  // J
};

uint8_t  grid[MATRIX_W][MATRIX_H];
int8_t   curX;
int16_t  curY;
uint8_t  curPiece;
uint8_t  curRot;
uint8_t  nextPiece;
uint16_t linesCleared = 0;
unsigned long fallTimer = 0;

unsigned long lastBtnTime[4] = {0,0,0,0};
bool prevBtn[4] = {false,false,false,false};

void getRotatedBlocks(uint8_t p, uint8_t rot, int8_t outX[4], int8_t outY[4]) {
  int8_t minX = 127, minY = 127;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t dx = PIECE_BLOCKS[p][i][0];
    int8_t dy = PIECE_BLOCKS[p][i][1];
    switch (rot & 3) {
      case 0: outX[i] = dx;  outY[i] = dy;  break;
      case 1: outX[i] = dy;  outY[i] = -dx; break;
      case 2: outX[i] = -dx; outY[i] = -dy; break;
      case 3: outX[i] = -dy; outY[i] = dx;  break;
    }
    if (outX[i] < minX) minX = outX[i];
    if (outY[i] < minY) minY = outY[i];
  }
  for (uint8_t i = 0; i < 4; i++) {
    outX[i] -= minX;
    outY[i] -= minY;
  }
}

bool canPlaceRot(int8_t px, int16_t py, uint8_t p, uint8_t rot) {
  int8_t bx[4], by_[4];
  getRotatedBlocks(p, rot, bx, by_);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t  gx = px + bx[i];
    int16_t gy = py + by_[i];
    if (gx < 0 || gx >= (int8_t)PLAY_W) return false;
    if (gy >= (int16_t)PLAY_H) return false;
    if (gy >= 0 && grid[gx][gy]) return false;
  }
  return true;
}

void spawnPiece() {
  curPiece = nextPiece;
  nextPiece = random(NUM_PIECES);
  curRot = 0;
  int8_t bxArr[4], byArr[4];
  getRotatedBlocks(curPiece, curRot, bxArr, byArr);
  int8_t minDx = 127, maxDx = -128;
  for (uint8_t i = 0; i < 4; i++) {
    if (bxArr[i] < minDx) minDx = bxArr[i];
    if (bxArr[i] > maxDx) maxDx = bxArr[i];
  }
  uint8_t pieceW = maxDx - minDx + 1;
  curX = (PLAY_W - pieceW) / 2 - minDx;
  curY = -3;
  fallTimer = millis();
}

void lockPiece() {
  int8_t bx[4], by_[4];
  getRotatedBlocks(curPiece, curRot, bx, by_);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t  gx = curX + bx[i];
    int16_t gy = curY + by_[i];
    if (gx >= 0 && gx < (int8_t)PLAY_W && gy >= 0 && gy < (int16_t)PLAY_H) {
      grid[gx][gy] = curPiece + 1;
    }
  }
}

void clearFullRows() {
  for (int16_t y = PLAY_H - 1; y >= 0; y--) {
    bool full = true;
    for (uint8_t x = 0; x < PLAY_W; x++) {
      if (!grid[x][y]) { full = false; break; }
    }
    if (full) {
      linesCleared++;
      for (int16_t row = y; row > 0; row--) {
        for (uint8_t x = 0; x < PLAY_W; x++) grid[x][row] = grid[x][row - 1];
      }
      for (uint8_t x = 0; x < PLAY_W; x++) grid[x][0] = 0;
      y++;
    }
  }
}

bool isGridFull() {
  uint8_t count = 0;
  for (uint8_t x = 0; x < PLAY_W; x++) if (grid[x][0]) count++;
  return count > PLAY_W / 2;
}

void renderBar() {
  uint16_t filled    = linesCleared % (BAR_PIX + 1);
  uint8_t  barColor  = (linesCleared / BAR_PIX) & 7;
  static const uint8_t BAR_COLORS[8] = {
    pack(1,0,0), pack(0,1,0), pack(0,0,1), pack(0,1,1),
    pack(1,0,1), pack(1,1,0), pack(1,1,1), pack(1,1,1),
  };
  uint8_t col = BAR_COLORS[barColor];
  for (uint16_t i = 0; i < filled; i++) {
    screen_panel::setPixel(BAR_X, PLAY_H - 1 - i, col);
  }
}

void render() {
  screen_panel::clearAll();
  for (uint8_t x = 0; x < PLAY_W; x++) {
    for (uint8_t y = 0; y < PLAY_H; y++) {
      if (grid[x][y]) {
        screen_panel::setPixel(x, y, PIECE_COL_DIM[grid[x][y] - 1]);
      }
    }
  }
  int8_t rbx[4], rby[4];
  getRotatedBlocks(curPiece, curRot, rbx, rby);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t  bx = curX + rbx[i];
    int16_t by = curY + rby[i];
    if (bx >= 0 && bx < (int8_t)PLAY_W && by >= 0 && by < (int16_t)PLAY_H) {
      screen_panel::setPixel((uint8_t)bx, (uint8_t)by, PIECE_COL[curPiece]);
    }
  }
  // Next-piece preview at the top-right (cols 12..14, rows 0..2).
  int8_t nx[4], ny[4];
  getRotatedBlocks(nextPiece, 0, nx, ny);
  uint8_t nextCol = PIECE_COL[nextPiece];
  for (uint8_t i = 0; i < 4; i++) {
    screen_panel::setPixel(PLAY_W + nx[i], ny[i], nextCol);
  }
  renderBar();
  screen_panel::show();
}

void startupBlink() {
  // Quick R / G / B / off flash across the whole matrix.
  const uint8_t seq[4] = {pack(1,0,0), pack(0,1,0), pack(0,0,1), 0};
  for (uint8_t s = 0; s < 4; s++) {
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      for (uint8_t y = 0; y < MATRIX_H; y++) screen_panel::setPixel(x, y, seq[s]);
    }
    screen_panel::show();
    delay(120);
  }
}

void gameSetup() {
  randomSeed(esp_random());
  memset(grid, 0, sizeof(grid));
  linesCleared = 0;
  nextPiece = random(NUM_PIECES);
  spawnPiece();
  render();
}

void gameTick() {
  unsigned long now = millis();

  bool btn[4];
  btn[0] = peripherals::buttonLeft();
  btn[1] = peripherals::buttonRight();
  btn[2] = peripherals::buttonRotate();
  btn[3] = peripherals::buttonDrop();

  bool moved = false;

  if (btn[0] && (now - lastBtnTime[0] >= DEBOUNCE_MS)) {
    if (canPlaceRot(curX - 1, curY, curPiece, curRot)) { curX--; moved = true; }
    lastBtnTime[0] = now;
  }
  if (!btn[0]) lastBtnTime[0] = now - DEBOUNCE_MS;

  if (btn[1] && (now - lastBtnTime[1] >= DEBOUNCE_MS)) {
    if (canPlaceRot(curX + 1, curY, curPiece, curRot)) { curX++; moved = true; }
    lastBtnTime[1] = now;
  }
  if (!btn[1]) lastBtnTime[1] = now - DEBOUNCE_MS;

  // Rotate: edge-triggered.
  if (btn[2] && !prevBtn[2]) {
    uint8_t newRot = (curRot + 1) & 3;
    if (canPlaceRot(curX, curY, curPiece, newRot)) { curRot = newRot; moved = true; }
  }
  prevBtn[2] = btn[2];

  if (moved) render();

  unsigned long speed = btn[3] ? FAST_FALL_MS : FALL_MS;
  if (now - fallTimer >= speed) {
    fallTimer = now;
    if (canPlaceRot(curX, curY + 1, curPiece, curRot)) {
      curY++;
    } else {
      lockPiece();
      clearFullRows();
      if (isGridFull()) {
        screen_panel::clearAll();
        screen_panel::show();
        delay(300);
        memset(grid, 0, sizeof(grid));
        linesCleared = 0;
      }
      spawnPiece();
    }
    render();
  }
}

}  // namespace

// ─── WiFi maintenance (mirrors esp32-tictactoe/main.cpp) ────────────────────
static bool waitForWifi(uint32_t timeoutMs) {
  const uint32_t start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - start >= timeoutMs) return false;
    delay(200);
  }
  return true;
}

static void maintainWifiAndOta() {
  if (wifi_ota::inApMode) return;

  if (WiFi.status() == WL_CONNECTED) {
    if (!g_otaReady) {
      Serial.printf("WiFi: connected (background), IP=%s\n",
                    WiFi.localIP().toString().c_str());
      wifi_ota::setupArduinoOta();
      g_otaReady = true;
    }
    return;
  }

  g_otaReady = false;
  static uint32_t s_lastBeginMs = 0;
  const uint32_t nowMs = millis();
  if (s_lastBeginMs == 0 || nowMs - s_lastBeginMs > 10000) {
    s_lastBeginMs = nowMs;
    wifi_ota::beginWifiNonBlocking();
  }
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.printf("\n=== f1-screen boot, fw=%s, repo=%s ===\n",
                FIRMWARE_VERSION, OTA_REPO);

  // 1. Hardware up.
  peripherals::setupLed();
  peripherals::setupButtons();
  screen_panel::setup();

  // 2. Self-test flash on the matrix.
  startupBlink();

  // 3. WiFi-arm chord: hold LEFT+RIGHT at boot to do a blocking
  //    associate + GitHub self-update check before entering the game.
  bool armed = peripherals::bothSideButtonsPressed();
  if (armed) {
    Serial.println("WiFi: armed at boot (LEFT+RIGHT held)");
    if (waitForWifi(0) || wifi_ota::connectWifi()) {
      wifi_ota::checkAndUpdateFromGithub();
      wifi_ota::setupArduinoOta();
      g_otaReady = true;
    } else {
      Serial.println("WiFi: armed connect failed, continuing in game-only mode");
      wifi_ota::beginWifiNonBlocking();
    }
  } else {
    wifi_ota::beginWifiNonBlocking();
  }

  // 4. Reset the panel after startupBlink + any OTA UI took it over.
  screen_panel::clearAll();
  screen_panel::show();

  // 5. Game state.
  gameSetup();
}

void loop() {
  // 1. Service any in-flight OTA upload.
  wifi_ota::handleOta();

  // 2. Suspend the game while OTA is uploading so the radio gets all
  //    the CPU time it wants and the panel doesn't fight the OTA UI.
  if (!wifi_ota::inProgress) {
    gameTick();
  }

  // 3. Keep WiFi healthy in the background.
  maintainWifiAndOta();
}

