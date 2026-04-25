#include <Arduino.h>
#include "ws2812.h"

#define LED_PIN_A   6
#define LED_PIN_B   7
#define LEDS_PER_PANEL 256
#define PANEL_W     8
#define PANEL_H     32
#define MATRIX_W    16
#define MATRIX_H    32

// Compact pixel: 2 bits per channel packed as [RR GG BB xx]
// 0b00=0  0b01=85  0b10=170  0b11=255
uint8_t pixA[LEDS_PER_PANEL];  // pin 6: columns 0-7
uint8_t pixB[LEDS_PER_PANEL];  // pin 7: columns 8-15 (upside-down)

static const uint8_t EXPAND[] = {0, 5, 15, 30};

static void showAll() {
  ws2812_showCompact(LED_PIN_A, pixA, LEDS_PER_PANEL, EXPAND);
  ws2812_showCompact(LED_PIN_B, pixB, LEDS_PER_PANEL, EXPAND);
}

static void clearAll() {
  memset(pixA, 0, sizeof(pixA));
  memset(pixB, 0, sizeof(pixB));
}

// Panel A row 0: A7 A6 A5 A4 A3 A2 A1 A0  (even rows: right-to-left)
// Panel A row 1: A8 A9 A10 A11 A12 A13 A14 A15 (odd rows: left-to-right)
// Panel B (flipped vertically): top row shows B120..B127
void setPixel(uint8_t x, uint8_t y, uint8_t packed) {
  if (x >= MATRIX_W || y >= MATRIX_H) return;
  if (x < PANEL_W) {
    // Panel A – even rows R→L, odd rows L→R
    uint16_t idx;
    if (y & 1) idx = y * PANEL_W + x;                    // odd: L→R
    else        idx = y * PANEL_W + (PANEL_W - 1 - x);   // even: R→L
    pixA[idx] = packed;
  } else {
    // Panel B – flipped vertically + columns mirrored
    uint8_t px = PANEL_W - 1 - (x - PANEL_W);  // mirror X
    uint8_t py = PANEL_H - 1 - y;                // flip Y
    uint16_t idx;
    if (py & 1) idx = py * PANEL_W + px;                  // odd: L→R
    else         idx = py * PANEL_W + (PANEL_W - 1 - px); // even: R→L
    pixB[idx] = packed;
  }
}

// Piece colours as packed bytes: I=Cyan O=Yellow T=Purple S=Green Z=Red L=Orange J=Blue
//                                  RR GG BB
//                          RR GG BB xx
const uint8_t PIECE_COL[] PROGMEM = {
  (0<<6)|(1<<4)|(1<<2),  // I  Cyan
  (1<<6)|(1<<4)|(0<<2),  // O  Yellow
  (1<<6)|(0<<4)|(1<<2),  // T  Purple
  (0<<6)|(1<<4)|(0<<2),  // S  Green
  (1<<6)|(0<<4)|(0<<2),  // Z  Red
  (1<<6)|(1<<4)|(0<<2),  // L  Orange
  (0<<6)|(0<<4)|(1<<2),  // J  Blue
};
// Dimmer version for locked pieces
const uint8_t PIECE_COL_DIM[] PROGMEM = {
  (0<<6)|(1<<4)|(1<<2),
  (1<<6)|(1<<4)|(0<<2),
  (1<<6)|(0<<4)|(1<<2),
  (0<<6)|(1<<4)|(0<<2),
  (1<<6)|(0<<4)|(0<<2),
  (1<<6)|(1<<4)|(0<<2),
  (0<<6)|(0<<4)|(1<<2),
};
constexpr uint8_t NUM_PIECES = 7;

// Each piece: 4 blocks as {dx, dy} offsets
const int8_t PIECE_BLOCKS[][4][2] PROGMEM = {
  {{0,0},{1,0},{2,0},{3,0}},  // I
  {{0,0},{1,0},{0,1},{1,1}},  // O
  {{0,0},{1,0},{2,0},{1,1}},  // T
  {{1,0},{2,0},{0,1},{1,1}},  // S
  {{0,0},{1,0},{1,1},{2,1}},  // Z
  {{0,0},{0,1},{0,2},{1,2}},  // L
  {{1,0},{1,1},{1,2},{0,2}},  // J
};

uint8_t grid[MATRIX_W][MATRIX_H];

int8_t curX;
int16_t curY;
uint8_t curPiece;
unsigned long fallTimer;
constexpr unsigned long FALL_MS = 50;

int8_t blk(uint8_t p, uint8_t i, uint8_t a) {
  return (int8_t)pgm_read_byte(&PIECE_BLOCKS[p][i][a]);
}

void spawnPiece() {
  curPiece = random(NUM_PIECES);
  int8_t minDx = 127, maxDx = -128;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t dx = blk(curPiece, i, 0);
    if (dx < minDx) minDx = dx;
    if (dx > maxDx) maxDx = dx;
  }
  curX = random(-minDx, MATRIX_W - maxDx);
  curY = -2;
  fallTimer = millis();
}

bool canPlace(int8_t px, int16_t py, uint8_t p) {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = px + blk(p, i, 0);
    int16_t by = py + blk(p, i, 1);
    if (bx < 0 || bx >= MATRIX_W) return false;
    if (by >= (int16_t)MATRIX_H) return false;
    if (by >= 0 && grid[bx][by]) return false;
  }
  return true;
}

void lockPiece() {
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = curX + blk(curPiece, i, 0);
    int16_t by = curY + blk(curPiece, i, 1);
    if (bx >= 0 && bx < (int8_t)MATRIX_W && by >= 0 && by < (int16_t)MATRIX_H) {
      grid[bx][by] = curPiece + 1;
    }
  }
}

void clearFullRows() {
  for (int16_t y = MATRIX_H - 1; y >= 0; y--) {
    bool full = true;
    for (uint8_t x = 0; x < MATRIX_W; x++) {
      if (!grid[x][y]) { full = false; break; }
    }
    if (full) {
      for (int16_t row = y; row > 0; row--) {
        for (uint8_t x = 0; x < MATRIX_W; x++) grid[x][row] = grid[x][row - 1];
      }
      for (uint8_t x = 0; x < MATRIX_W; x++) grid[x][0] = 0;
      y++;
    }
  }
}

bool isGridFull() {
  uint8_t count = 0;
  for (uint8_t x = 0; x < MATRIX_W; x++) {
    if (grid[x][0]) count++;
  }
  return count > MATRIX_W / 2;
}

void render() {
  clearAll();
  for (uint8_t x = 0; x < MATRIX_W; x++) {
    for (uint8_t y = 0; y < MATRIX_H; y++) {
      if (grid[x][y]) {
        setPixel(x, y, pgm_read_byte(&PIECE_COL_DIM[grid[x][y] - 1]));
      }
    }
  }
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = curX + blk(curPiece, i, 0);
    int16_t by = curY + blk(curPiece, i, 1);
    if (bx >= 0 && bx < (int8_t)MATRIX_W && by >= 0 && by < (int16_t)MATRIX_H) {
      setPixel((uint8_t)bx, (uint8_t)by, pgm_read_byte(&PIECE_COL[curPiece]));
    }
  }
  showAll();
}

void setup() {
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  clearAll();
  showAll();
  randomSeed(analogRead(A0));
  memset(grid, 0, sizeof(grid));
  spawnPiece();
}

void loop() {
  unsigned long now = millis();
  if (now - fallTimer >= FALL_MS) {
    fallTimer = now;
    if (canPlace(curX, curY + 1, curPiece)) {
      curY++;
    } else {
      lockPiece();
      clearFullRows();
      if (isGridFull()) {
        clearAll();
        showAll();
        delay(300);
        memset(grid, 0, sizeof(grid));
      }
      spawnPiece();
    }
    render();
  }
}
