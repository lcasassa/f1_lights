#include <Arduino.h>
#include "ws2812.h"

#define LED_PIN_A   6
#define LED_PIN_B   7
#define LEDS_PER_PANEL 256
#define PANEL_W     8
#define PANEL_H     32
#define PHYS_W      16   // physical matrix width
#define PHYS_H      32   // physical matrix height

// Game uses rotated coordinates: 32 wide x 16 tall
#define GAME_W      32
#define GAME_H      16

#define BTN_JUMP    2
#define BTN_DUCK    4

#define GROUND_Y    (GAME_H - 1)  // bottom row = ground
#define DINO_X      3              // dino fixed X position

// Compact pixel buffers
uint8_t pixA[LEDS_PER_PANEL];
uint8_t pixB[LEDS_PER_PANEL];

static const uint8_t EXPAND[] = {0, 5, 15, 30};

static void showAll() {
  ws2812_showCompact(LED_PIN_A, pixA, LEDS_PER_PANEL, EXPAND);
  ws2812_showCompact(LED_PIN_B, pixB, LEDS_PER_PANEL, EXPAND);
}

static void clearAll() {
  memset(pixA, 0, sizeof(pixA));
  memset(pixB, 0, sizeof(pixB));
}

// Physical pixel setter (unchanged from your panel mapping)
static void setPhysPixel(uint8_t x, uint8_t y, uint8_t packed) {
  if (x >= PHYS_W || y >= PHYS_H) return;
  if (x < PANEL_W) {
    uint16_t idx;
    if (y & 1) idx = y * PANEL_W + x;
    else        idx = y * PANEL_W + (PANEL_W - 1 - x);
    pixA[idx] = packed;
  } else {
    uint8_t px = PANEL_W - 1 - (x - PANEL_W);
    uint8_t py = PANEL_H - 1 - y;
    uint16_t idx;
    if (py & 1) idx = py * PANEL_W + px;
    else         idx = py * PANEL_W + (PANEL_W - 1 - px);
    pixB[idx] = packed;
  }
}

// Game pixel: rotate 90° CW so game X (0..31) runs along the 32-pixel physical Y axis
// Game (gx, gy) -> Physical (gy, PHYS_H - 1 - gx)
void setPixel(uint8_t gx, uint8_t gy, uint8_t packed) {
  if (gx >= GAME_W || gy >= GAME_H) return;
  setPhysPixel(gy, PHYS_H - 1 - gx, packed);
}

// Colors (packed 2-bit RGB)
#define COL_WHITE   ((1<<6)|(1<<4)|(1<<2))
#define COL_GREEN   ((0<<6)|(1<<4)|(0<<2))
#define COL_RED     ((1<<6)|(0<<4)|(0<<2))
#define COL_YELLOW  ((1<<6)|(1<<4)|(0<<2))
#define COL_CYAN    ((0<<6)|(1<<4)|(1<<2))
#define COL_GROUND  ((0<<6)|(0<<4)|(1<<2))  // dim blue ground line
#define COL_DINO    ((0<<6)|(1<<4)|(0<<2))  // green dino
#define COL_CACTUS  ((1<<6)|(0<<4)|(0<<2))  // red cactus
#define COL_BIRD    ((1<<6)|(1<<4)|(0<<2))  // yellow bird
#define COL_SCORE   ((1<<6)|(1<<4)|(1<<2))  // white score

// --- Dino sprite (5 wide x 6 tall, origin at bottom-left foot) ---
// Standing dino (legs down)
const uint8_t DINO_STAND[] PROGMEM = {
  // row offsets from bottom: dy=0 is feet, dy=5 is top of head
  // Each byte: packed x positions as bits (bit 0=x+0, bit 1=x+1, etc.)
  0b00110,  // dy=0: feet (x+1, x+2)
  0b00110,  // dy=1: legs
  0b01110,  // dy=2: body (x+1, x+2, x+3)
  0b11110,  // dy=3: body+arm (x+0..x+3) -- arm sticks out
  0b01110,  // dy=4: neck
  0b01100,  // dy=5: head (x+1, x+2)
};
#define DINO_H 6
#define DINO_W 5

// Ducking dino (3 tall, wider)
const uint8_t DINO_DUCK[] PROGMEM = {
  0b01110,  // dy=0: feet
  0b11111,  // dy=1: long body
  0b01100,  // dy=2: head
};
#define DUCK_H 3

// --- Obstacles ---
#define MAX_OBSTACLES 6  // more room now with 32 wide

struct Obstacle {
  int16_t x;
  uint8_t type;
  bool active;
};

Obstacle obstacles[MAX_OBSTACLES];

// Cactus shapes: small (2w x 3h), tall (2w x 5h)
// Bird shape: 3w x 2h

// --- Game state ---
int8_t dinoY;
int8_t dinoVelY;
bool isDucking;
bool isJumping;
uint16_t score;
uint16_t hiScore;
unsigned long gameTimer;
unsigned long obstacleTimer;
unsigned long frameTimer;
uint8_t gameSpeed;
bool gameOver;
bool waitingToStart;

#define GRAVITY      1
#define JUMP_VEL    -3
#define MIN_SPEED   35
#define START_SPEED 70
#define SPEED_STEP   1

// --- Draw functions (using GAME_W / GAME_H now) ---

void drawGround() {
  for (uint8_t x = 0; x < GAME_W; x++) {
    setPixel(x, GROUND_Y, COL_GROUND);
  }
}

void drawDino() {
  uint8_t footY = dinoY;
  if (isDucking && !isJumping) {
    // Draw ducking sprite
    for (uint8_t dy = 0; dy < DUCK_H; dy++) {
      uint8_t row = pgm_read_byte(&DINO_DUCK[dy]);
      for (uint8_t dx = 0; dx < DINO_W; dx++) {
        if (row & (1 << dx)) {
          int8_t py = footY - dy;
          if (py >= 0 && py < GAME_H)
            setPixel(DINO_X + dx, py, COL_DINO);
        }
      }
    }
  } else {
    // Draw standing/jumping sprite
    for (uint8_t dy = 0; dy < DINO_H; dy++) {
      uint8_t row = pgm_read_byte(&DINO_STAND[dy]);
      for (uint8_t dx = 0; dx < DINO_W; dx++) {
        if (row & (1 << dx)) {
          int8_t py = footY - dy;
          if (py >= 0 && py < GAME_H)
            setPixel(DINO_X + dx, py, COL_DINO);
        }
      }
    }
  }
}

void drawObstacle(const Obstacle &ob) {
  if (!ob.active || ob.x >= GAME_W || ob.x < -4) return;

  switch (ob.type) {
    case 0: // small cactus: 2w x 3h on ground
      for (int8_t dy = 0; dy < 3; dy++) {
        for (int8_t dx = 0; dx < 2; dx++) {
          int16_t px = ob.x + dx;
          int8_t py = GROUND_Y - 1 - dy;
          if (px >= 0 && px < GAME_W && py >= 0)
            setPixel(px, py, COL_CACTUS);
        }
      }
      break;
    case 1: // tall cactus: 2w x 5h on ground
      for (int8_t dy = 0; dy < 5; dy++) {
        for (int8_t dx = 0; dx < 2; dx++) {
          int16_t px = ob.x + dx;
          int8_t py = GROUND_Y - 1 - dy;
          if (px >= 0 && px < GAME_W && py >= 0)
            setPixel(px, py, COL_CACTUS);
        }
      }
      // arms
      if (ob.x - 1 >= 0) setPixel(ob.x - 1, GROUND_Y - 3, COL_CACTUS);
      if (ob.x + 2 < GAME_W) setPixel(ob.x + 2, GROUND_Y - 2, COL_CACTUS);
      break;
    case 2: // bird low: 3w x 2h, flies at ground+3
      for (int8_t dx = 0; dx < 3; dx++) {
        int16_t px = ob.x + dx;
        if (px >= 0 && px < GAME_W) {
          setPixel(px, GROUND_Y - 3, COL_BIRD);
          if (dx == 1) setPixel(px, GROUND_Y - 4, COL_BIRD); // wing up
        }
      }
      break;
    case 3: // bird high: 3w x 2h, flies at ground+7
      for (int8_t dx = 0; dx < 3; dx++) {
        int16_t px = ob.x + dx;
        if (px >= 0 && px < GAME_W) {
          setPixel(px, GROUND_Y - 7, COL_BIRD);
          if (dx == 1) setPixel(px, GROUND_Y - 8, COL_BIRD);
        }
      }
      break;
  }
}

// Draw score as vertical bar on column 15 (like the tetris bar)
void drawScore() {
  // Score bar along the top row (y=0) filling left to right
  uint8_t filled = score % (GAME_W + 1);
  uint8_t cycle = (score / GAME_W) & 7;
  static const uint8_t SCORE_COLORS[] = {
    COL_WHITE, COL_GREEN, COL_CYAN, COL_YELLOW,
    COL_RED, ((1<<6)|(0<<4)|(1<<2)), COL_WHITE, COL_WHITE
  };
  uint8_t col = SCORE_COLORS[cycle];
  for (uint8_t i = 0; i < filled; i++) {
    setPixel(i, 0, col);
  }
}

// --- Collision detection ---

bool dinoCollides(const Obstacle &ob) {
  if (!ob.active) return false;

  // Get dino bounding box
  int8_t dLeft = DINO_X;
  int8_t dRight, dTop, dBottom;
  dBottom = dinoY;

  if (isDucking && !isJumping) {
    dRight = DINO_X + DINO_W - 1;
    dTop = dinoY - (DUCK_H - 1);
  } else {
    dRight = DINO_X + DINO_W - 1;
    dTop = dinoY - (DINO_H - 1);
  }

  // Get obstacle bounding box
  int8_t oLeft, oRight, oTop, oBottom;
  switch (ob.type) {
    case 0: // small cactus
      oLeft = ob.x; oRight = ob.x + 1;
      oBottom = GROUND_Y - 1; oTop = GROUND_Y - 3;
      break;
    case 1: // tall cactus
      oLeft = ob.x - 1; oRight = ob.x + 2;
      oBottom = GROUND_Y - 1; oTop = GROUND_Y - 5;
      break;
    case 2: // bird low
      oLeft = ob.x; oRight = ob.x + 2;
      oBottom = GROUND_Y - 3; oTop = GROUND_Y - 4;
      break;
    case 3: // bird high
      oLeft = ob.x; oRight = ob.x + 2;
      oBottom = GROUND_Y - 7; oTop = GROUND_Y - 8;
      break;
    default: return false;
  }

  // AABB overlap check
  if (dRight < oLeft || dLeft > oRight) return false;
  if (dBottom < oTop || dTop > oBottom) return false;
  return true;
}

// --- Game logic ---

void resetGame() {
  dinoY = GROUND_Y - 1;  // feet on ground (1 pixel above ground line)
  dinoVelY = 0;
  isDucking = false;
  isJumping = false;
  score = 0;
  gameSpeed = START_SPEED;
  gameOver = false;

  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
    obstacles[i].active = false;
  }

  obstacleTimer = millis();
  frameTimer = millis();
}

void spawnObstacle() {
  // Find inactive slot
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
    if (!obstacles[i].active) {
      obstacles[i].active = true;
      obstacles[i].x = GAME_W;  // spawn at right edge
      // Random type: 60% small cactus, 20% tall cactus, 10% bird low, 10% bird high
      uint8_t r = random(100);
      if (r < 50)       obstacles[i].type = 0;
      else if (r < 80)  obstacles[i].type = 1;
      else if (r < 90)  obstacles[i].type = 2;
      else               obstacles[i].type = 3;
      return;
    }
  }
}

void updateGame() {
  // Jump physics
  if (isJumping) {
    dinoY += dinoVelY;
    dinoVelY += GRAVITY;
    if (dinoY >= GROUND_Y - 1) {
      dinoY = GROUND_Y - 1;
      dinoVelY = 0;
      isJumping = false;
    }
  }

  // Move obstacles left
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
    if (obstacles[i].active) {
      obstacles[i].x--;
      if (obstacles[i].x < -4) {
        obstacles[i].active = false;
        score++;
        // Speed up
        if (gameSpeed > MIN_SPEED) {
          gameSpeed -= SPEED_STEP;
        }
      }
    }
  }

  // Spawn new obstacles
  unsigned long now = millis();
  uint16_t spawnDelay = (uint16_t)gameSpeed * 10 + random(gameSpeed * 8);
  if (now - obstacleTimer >= spawnDelay) {
    spawnObstacle();
    obstacleTimer = now;
  }

  // Check collisions
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) {
    if (dinoCollides(obstacles[i])) {
      gameOver = true;
      if (score > hiScore) hiScore = score;
      return;
    }
  }
}

void render() {
  clearAll();
  drawGround();
  drawDino();
  for (uint8_t i = 0; i < MAX_OBSTACLES; i++) drawObstacle(obstacles[i]);
  drawScore();
  showAll();
}

// Flash screen on game over
void gameOverAnimation() {
  for (uint8_t flash = 0; flash < 3; flash++) {
    // Flash red
    for (uint16_t i = 0; i < LEDS_PER_PANEL; i++) {
      pixA[i] = COL_RED;
      pixB[i] = COL_RED;
    }
    showAll();
    delay(150);
    clearAll();
    showAll();
    delay(150);
  }
  // Show final score for a moment
  render();
  delay(1000);
}

void setup() {
  pinMode(LED_PIN_A, OUTPUT);
  pinMode(LED_PIN_B, OUTPUT);
  pinMode(BTN_JUMP, INPUT_PULLUP);
  pinMode(BTN_DUCK, INPUT_PULLUP);
  clearAll();
  showAll();
  Serial.begin(115200);
  Serial.println(F("Dino Game (rotated 32x16)"));
  randomSeed(analogRead(A0));
  hiScore = 0;
  waitingToStart = true;
  resetGame();
}

bool prevJump = false;

void loop() {
  unsigned long now = millis();

  bool btnJump = !digitalRead(BTN_JUMP);
  bool btnDuck = !digitalRead(BTN_DUCK);

  if (waitingToStart) {
    // Show dino standing, waiting for jump to start
    clearAll();
    drawGround();
    dinoY = GROUND_Y - 1;
    drawDino();
    // Blink "press jump" — draw a little arrow on right side
    if ((now / 500) & 1) {
      setPixel(10, GROUND_Y - 3, COL_WHITE);
      setPixel(11, GROUND_Y - 4, COL_WHITE);
      setPixel(12, GROUND_Y - 3, COL_WHITE);
      setPixel(11, GROUND_Y - 5, COL_WHITE);
    }
    showAll();

    if (btnJump && !prevJump) {
      waitingToStart = false;
      resetGame();
      // Start with a jump
      isJumping = true;
      dinoVelY = JUMP_VEL;
    }
    prevJump = btnJump;
    return;
  }

  if (gameOver) {
    gameOverAnimation();
    waitingToStart = true;
    return;
  }

  // Handle jump input — edge detect
  if (btnJump && !prevJump && !isJumping) {
    isJumping = true;
    dinoVelY = JUMP_VEL;
  }
  prevJump = btnJump;

  // Handle duck input
  isDucking = btnDuck;

  // Game tick
  if (now - frameTimer >= gameSpeed) {
    frameTimer = now;
    updateGame();
    if (!gameOver) render();
  }
}
