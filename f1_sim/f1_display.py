"""
F1 Start Lights — pygame display.

Controls:
  Hold 1  — Left button  (top row player)
  Hold 2  — Right button (bottom row player)
  Escape  — Quit

The Arduino loop() is called every 20 ms of real time.
"""

import subprocess
import sys
import time

import pygame

from . import F1Sim, _PROJECT_ROOT

# ── Layout constants ─────────────────────────────────────────────────────────
LEDS_PER_ROW   = 5
ROWS           = 2
LED_RADIUS     = 36
LED_SPACING    = 100          # centre-to-centre
PADDING        = 60           # left/right margin
ROW_GAP        = 110          # vertical gap between row centres

WINDOW_W = PADDING * 2 + (LEDS_PER_ROW - 1) * LED_SPACING + LED_RADIUS * 2
WINDOW_H = PADDING * 2 + (ROWS - 1) * ROW_GAP + LED_RADIUS * 2 + 80  # +80 for label area

LOOP_INTERVAL_MS = 20         # call loop() every 20 ms

# ── Colours ──────────────────────────────────────────────────────────────────
BG_COLOUR      = (15,  15,  15)
LED_OFF        = (60,  10,  10)
LED_ON         = (255,  30,  30)
LED_GLOW       = (255, 120,  60)
TEXT_COLOUR    = (200, 200, 200)
DIM_TEXT       = (100, 100, 100)
KEY_ACTIVE     = (220, 220,  60)
KEY_INACTIVE   = (80,   80,  80)

# LED positions: row 0 = POS 1-5 (top), row 1 = POS 6-10 (bottom)
def _led_centre(row: int, col: int) -> tuple[int, int]:
    x = PADDING + LED_RADIUS + col * LED_SPACING
    y = PADDING + LED_RADIUS + row * ROW_GAP
    return x, y


def _draw_led(surface: pygame.Surface, centre: tuple[int, int], on: bool):
    x, y = centre
    colour = LED_ON if on else LED_OFF
    if on:
        # soft glow — just slightly larger than the LED to avoid overlap
        glow_r = LED_RADIUS + 1
        glow_surf = pygame.Surface((glow_r * 2, glow_r * 2), pygame.SRCALPHA)
        pygame.draw.circle(glow_surf, (*LED_GLOW, 60), (glow_r, glow_r), glow_r)
        surface.blit(glow_surf, (x - glow_r, y - glow_r))
    pygame.draw.circle(surface, colour, (x, y), LED_RADIUS)
    pygame.draw.circle(surface, (255, 255, 255) if on else (90, 30, 30), (x, y), LED_RADIUS, 2)


def run():
    # Ensure lib is up to date
    subprocess.check_call(["make", "sim"], cwd=_PROJECT_ROOT)

    sim = F1Sim()
    sim.reset()
    sim.set_millis(0)
    sim.setup()

    pygame.init()
    screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
    pygame.display.set_caption("F1 Start Lights")
    clock = pygame.time.Clock()

    font_label = pygame.font.SysFont("monospace", 15)
    font_hint  = pygame.font.SysFont("monospace", 17)
    font_title = pygame.font.SysFont("monospace", 20, bold=True)

    last_loop_ms = time.monotonic() * 1000
    sim_time_ms  = 0

    running = True
    while running:
        # ── Events ───────────────────────────────────────────────────────────
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN and event.key == pygame.K_ESCAPE:
                running = False

        # ── Button state from held keys ───────────────────────────────────
        keys = pygame.key.get_pressed()
        if keys[pygame.K_1]:
            sim.press_left()
        else:
            sim.release_left()

        if keys[pygame.K_2]:
            sim.press_right()
        else:
            sim.release_right()

        # ── Advance sim clock and call loop() every LOOP_INTERVAL_MS ─────
        now_ms = time.monotonic() * 1000
        if now_ms - last_loop_ms >= LOOP_INTERVAL_MS:
            sim_time_ms += LOOP_INTERVAL_MS
            sim.set_millis(sim_time_ms)
            sim.loop()
            last_loop_ms = now_ms

        # ── Draw ─────────────────────────────────────────────────────────
        screen.fill(BG_COLOUR)

        # Title
        title = font_title.render("F1 START LIGHTS", True, TEXT_COLOUR)
        screen.blit(title, (WINDOW_W // 2 - title.get_width() // 2, 10))

        # LEDs — row 0: POS 1-5, row 1: POS 6-10
        for row in range(ROWS):
            for col in range(LEDS_PER_ROW):
                pos = row * LEDS_PER_ROW + col + 1   # 1-10
                centre = _led_centre(row, col)
                _draw_led(screen, centre, sim.led_state(pos))

                # Position label beneath LED
                lbl = font_label.render(str(pos), True, DIM_TEXT)
                screen.blit(lbl, (centre[0] - lbl.get_width() // 2, centre[1] + LED_RADIUS + 4))

        # Key indicators at the bottom
        hint_y = WINDOW_H - 40
        key1_col = KEY_ACTIVE if keys[pygame.K_1] else KEY_INACTIVE
        key2_col = KEY_ACTIVE if keys[pygame.K_2] else KEY_INACTIVE
        k1 = font_hint.render("[1]", True, key1_col)
        k2 = font_hint.render("[2]", True, key2_col)
        esc = font_hint.render("[ESC] quit", True, DIM_TEXT)
        screen.blit(k1,  (PADDING, hint_y))
        screen.blit(k2,  (PADDING + 140, hint_y))
        screen.blit(esc, (WINDOW_W - esc.get_width() - PADDING, hint_y))

        pygame.display.flip()
        clock.tick(60)   # cap at 60 fps; loop() fires on its own 20ms schedule

    pygame.quit()
    sys.exit(0)


if __name__ == "__main__":
    run()





