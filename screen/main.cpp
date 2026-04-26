#include <Arduino.h>
#include "ws2812.h"

#define LED_PIN_A   6
#define LED_PIN_B   7
#define LEDS_PER_PANEL 256
#define PANEL_W     8
#define PANEL_H     32
#define MATRIX_W    16
#define MATRIX_H    32

#define PLAY_H      32  // playable rows (0-11)
#define PLAY_W      12  // playable columns (0-11), last 4 cols reserved
#define BAR_X       15  // vertical bar on column 15 (far right)
#define BAR_PIX     PLAY_H  // bar height = play area height
#define BTN_LEFT    4
#define BTN_RIGHT   2
#define BTN_ROTATE  3
#define BTN_DROP    5

uint16_t linesCleared = 0;  // total lines cleared counter
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
uint8_t curRot;
uint8_t nextPiece;   // preview of next piece
int8_t targetX;    // best X to move toward
uint8_t targetRot; // best rotation to reach
unsigned long fallTimer;
constexpr unsigned long FALL_MS = 300;      // normal fall speed
constexpr unsigned long FAST_FALL_MS = 30;   // when drop button held
constexpr unsigned long INPUT_MS = 120;      // button repeat rate

int8_t blk(uint8_t p, uint8_t i, uint8_t a) {
  return (int8_t)pgm_read_byte(&PIECE_BLOCKS[p][i][a]);
}

// Get rotated block offset. rot: 0=normal, 1=CW90, 2=180, 3=CCW90
// Rotation around origin: (dx,dy) → CW90:(dy,-dx) → 180:(-dx,-dy) → CCW90:(-dy,dx)
// After rotation, normalize so min x and min y are 0.
void getRotatedBlocks(uint8_t p, uint8_t rot, int8_t outX[4], int8_t outY[4]) {
  int8_t minX = 127, minY = 127;
  for (uint8_t i = 0; i < 4; i++) {
    int8_t dx = blk(p, i, 0);
    int8_t dy = blk(p, i, 1);
    switch (rot & 3) {
      case 0: outX[i] = dx;  outY[i] = dy;  break;
      case 1: outX[i] = dy;  outY[i] = -dx; break;
      case 2: outX[i] = -dx; outY[i] = -dy; break;
      case 3: outX[i] = -dy; outY[i] = dx;  break;
    }
    if (outX[i] < minX) minX = outX[i];
    if (outY[i] < minY) minY = outY[i];
  }
  // Normalize to (0,0) origin
  for (uint8_t i = 0; i < 4; i++) {
    outX[i] -= minX;
    outY[i] -= minY;
  }
}

// Forward declaration
bool canPlaceRot(int8_t px, int16_t py, uint8_t p, uint8_t rot);

int16_t dropYRot(int8_t px, uint8_t p, uint8_t rot) {
  int16_t y = -3;
  while (canPlaceRot(px, y + 1, p, rot)) y++;
  return y;
}

uint16_t countHolesRot(int8_t px, int16_t py, uint8_t p, uint8_t rot) {
  int8_t bx[4], by_[4];
  getRotatedBlocks(p, rot, bx, by_);
  // Temporarily place
  for (uint8_t i = 0; i < 4; i++) {
    int8_t gx = px + bx[i];
    int16_t gy = py + by_[i];
    if (gx >= 0 && gx < PLAY_W && gy >= 0 && gy < PLAY_H)
      grid[gx][gy] = p + 1;
  }
  uint16_t holes = 0;
  for (uint8_t x = 0; x < PLAY_W; x++) {
    bool found = false;
    for (uint8_t y = 0; y < PLAY_H; y++) {
      if (grid[x][y]) found = true;
      else if (found) holes++;
    }
  }
  // Undo
  for (uint8_t i = 0; i < 4; i++) {
    int8_t gx = px + bx[i];
    int16_t gy = py + by_[i];
    if (gx >= 0 && gx < PLAY_W && gy >= 0 && gy < PLAY_H)
      grid[gx][gy] = 0;
  }
  return holes;
}

// Find the best placement for a piece (target X and rotation)
void findBestPlacement(uint8_t piece, int8_t &bestX, uint8_t &bestRot) {
  int16_t bestY = -100;
  uint16_t bestHoles = 0xFFFF;
  bestX = 0;
  bestRot = 0;

  for (uint8_t rot = 0; rot < 4; rot++) {
    int8_t bxArr[4], byArr[4];
    getRotatedBlocks(piece, rot, bxArr, byArr);
    int8_t minDx = 127, maxDx = -128;
    for (uint8_t i = 0; i < 4; i++) {
      if (bxArr[i] < minDx) minDx = bxArr[i];
      if (bxArr[i] > maxDx) maxDx = bxArr[i];
    }
    for (int8_t tx = -minDx; tx <= PLAY_W - 1 - maxDx; tx++) {
      int16_t ty = dropYRot(tx, piece, rot);
      uint16_t h = countHolesRot(tx, ty, piece, rot);
      if (h < bestHoles || (h == bestHoles && ty > bestY)) {
        bestHoles = h;
        bestY = ty;
        bestX = tx;
        bestRot = rot;
      }
    }
  }
}

void spawnPiece() {
  curPiece = nextPiece;
  nextPiece = random(NUM_PIECES);
  curRot = 0;
  // Start centered
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

bool canPlaceRot(int8_t px, int16_t py, uint8_t p, uint8_t rot) {
  int8_t bx[4], by_[4];
  getRotatedBlocks(p, rot, bx, by_);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t gx = px + bx[i];
    int16_t gy = py + by_[i];
    if (gx < 0 || gx >= PLAY_W) return false;
    if (gy >= (int16_t)PLAY_H) return false;
    if (gy >= 0 && grid[gx][gy]) return false;
  }
  return true;
}

void lockPiece() {
  int8_t bx[4], by_[4];
  getRotatedBlocks(curPiece, curRot, bx, by_);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t gx = curX + bx[i];
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
  for (uint8_t x = 0; x < PLAY_W; x++) {
    if (grid[x][0]) count++;
  }
  return count > PLAY_W / 2;
}

// Draw vertical progress bar on column BAR_X, fills bottom to top
void renderBar() {
  // Bar fills from bottom (row PLAY_H-1) to top (row 0)
  uint16_t filled = linesCleared % (BAR_PIX + 1);
  uint8_t barColor = ((linesCleared / BAR_PIX) & 7);
  static const uint8_t BAR_COLORS[] = {
    (1<<6)|(0<<4)|(0<<2),  // Red
    (0<<6)|(1<<4)|(0<<2),  // Green
    (0<<6)|(0<<4)|(1<<2),  // Blue
    (0<<6)|(1<<4)|(1<<2),  // Cyan
    (1<<6)|(0<<4)|(1<<2),  // Magenta
    (1<<6)|(1<<4)|(0<<2),  // Yellow
    (1<<6)|(1<<4)|(1<<2),  // White
    (1<<6)|(1<<4)|(1<<2),  // White
  };
  uint8_t col = BAR_COLORS[barColor];

  for (uint8_t i = 0; i < filled; i++) {
    setPixel(BAR_X, PLAY_H - 1 - i, col);  // bottom to top
  }
}

void render() {
  clearAll();
  // Draw grid (play area only)
  for (uint8_t x = 0; x < PLAY_W; x++) {
    for (uint8_t y = 0; y < PLAY_H; y++) {
      if (grid[x][y]) {
        setPixel(x, y, pgm_read_byte(&PIECE_COL_DIM[grid[x][y] - 1]));
      }
    }
  }
  int8_t rbx[4], rby[4];
  getRotatedBlocks(curPiece, curRot, rbx, rby);
  for (uint8_t i = 0; i < 4; i++) {
    int8_t bx = curX + rbx[i];
    int16_t by = curY + rby[i];
    if (bx >= 0 && bx < (int8_t)PLAY_W && by >= 0 && by < (int16_t)PLAY_H) {
      setPixel((uint8_t)bx, (uint8_t)by, pgm_read_byte(&PIECE_COL[curPiece]));
    }
  }
  // Draw next piece preview at far top-right (x=12..15, y=0..2)
  int8_t nx[4], ny[4];
  getRotatedBlocks(nextPiece, 0, nx, ny);
  uint8_t nextCol = pgm_read_byte(&PIECE_COL[nextPiece]);
  for (uint8_t i = 0; i < 4; i++) {
    setPixel(PLAY_W + nx[i], ny[i], nextCol);
  }
  renderBar();
  showAll();
}

void setup() {
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_ROTATE, INPUT_PULLUP);
  pinMode(BTN_DROP, INPUT_PULLUP);
  clearAll();
  showAll();
  Serial.begin(115200);
  Serial.println(F("Tetris starting..."));
  Serial.println(F("Pins: L=2 R=3 Rot=4 Drop=5"));
  randomSeed(analogRead(A0));
  memset(grid, 0, sizeof(grid));
  nextPiece = random(NUM_PIECES);
  spawnPiece();
}

unsigned long lastBtnTime[4] = {0,0,0,0};  // debounce per button
constexpr unsigned long DEBOUNCE_MS = 150;
bool prevBtn[4] = {false,false,false,false};

void loop() {
  unsigned long now = millis();

  // Read buttons (LOW = pressed with pull-up)
  bool btn[4];
  btn[0] = !digitalRead(BTN_LEFT);
  btn[1] = !digitalRead(BTN_RIGHT);
  btn[2] = !digitalRead(BTN_ROTATE);
  btn[3] = !digitalRead(BTN_DROP);

  // Debug: print when any button pressed
  static uint8_t prevDebug = 0;
  uint8_t curDebug = (btn[0]?1:0)|(btn[1]?2:0)|(btn[2]?4:0)|(btn[3]?8:0);
  if (curDebug != prevDebug) {
    Serial.print(F("BTN: L="));Serial.print(btn[0]);
    Serial.print(F(" R="));Serial.print(btn[1]);
    Serial.print(F(" Rot="));Serial.print(btn[2]);
    Serial.print(F(" Drop="));Serial.println(btn[3]);
    prevDebug = curDebug;
  }

  bool moved = false;

  // Left — repeat while held
  if (btn[0] && (now - lastBtnTime[0] >= DEBOUNCE_MS)) {
    if (canPlaceRot(curX - 1, curY, curPiece, curRot)) {
      curX--;
      moved = true;
    }
    lastBtnTime[0] = now;
  }
  if (!btn[0]) lastBtnTime[0] = now - DEBOUNCE_MS; // allow immediate next press

  // Right — repeat while held
  if (btn[1] && (now - lastBtnTime[1] >= DEBOUNCE_MS)) {
    if (canPlaceRot(curX + 1, curY, curPiece, curRot)) {
      curX++;
      moved = true;
    }
    lastBtnTime[1] = now;
  }
  if (!btn[1]) lastBtnTime[1] = now - DEBOUNCE_MS;

  // Rotate — edge detect only (press, not hold)
  if (btn[2] && !prevBtn[2]) {
    uint8_t newRot = (curRot + 1) & 3;
    if (canPlaceRot(curX, curY, curPiece, newRot)) {
      curRot = newRot;
      moved = true;
    }
  }
  prevBtn[2] = btn[2];

  // Render immediately on any move
  if (moved) render();

  // Fall speed depends on drop button
  unsigned long speed = btn[3] ? FAST_FALL_MS : FALL_MS;

  if (now - fallTimer >= speed) {
    fallTimer = now;
    if (canPlaceRot(curX, curY + 1, curPiece, curRot)) {
      curY++;
    } else {
      lockPiece();
      clearFullRows();
      if (isGridFull()) {
        clearAll();
        showAll();
        delay(300);
        memset(grid, 0, sizeof(grid));
        linesCleared = 0;
      }
      spawnPiece();
    }
    render();
  }
}
