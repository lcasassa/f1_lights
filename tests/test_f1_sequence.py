"""Test the full F1 start-light game sequence via the desktop simulation."""

import pytest
from f1_sim import F1Sim


def start_sequence(sim: F1Sim):
    """Press both buttons, release both, wait for LIGHTING_UP to begin."""
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # IDLE/WINNER_WAIT_RESTART -> WAIT_RELEASE
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()   # WAIT_RELEASE -> LIGHTING_UP, column 1 lights up
    sim.advance_millis(1)
    sim.loop()   # LIGHTING_UP: column 1 on


@pytest.mark.parametrize("early_side, expected_top, expected_bottom, penalty_pos", [
    # Left early start: right row wins + POS-3 (top row, index 2) lit as left's penalty
    ("left",  [False, False, True, False, False], [True, True, True, True, True], 3),
    # Right early start: left row wins + POS-8 (bottom row, index 2) lit as right's penalty
    ("right", [True, True, True, True, True], [False, False, True, False, False], 8),
])
def test_early_start(sim: F1Sim, early_side, expected_top, expected_bottom, penalty_pos):
    """Early button press during LIGHTING_UP disqualifies that player.
    The opposing row lights up, and the guilty player's middle LED is lit as a penalty.
    """
    # Start sequence (press both, release both, column 1 on)
    sim.advance_millis(100)
    start_sequence(sim)

    # Column 1 is now on — sequence is in progress
    assert sim.led_state(1) is True
    assert sim.led_state(6) is True

    # Trigger early start during LIGHTING_UP (column 2 not yet lit)
    if early_side == "left":
        sim.press_left()
    else:
        sim.press_right()
    sim.advance_millis(15)
    sim.loop()

    # Opposing player's row is on
    assert sim.top_row() == expected_top
    assert sim.bottom_row() == expected_bottom

    # Penalty LED (middle LED of the guilty player's row) is also lit
    assert sim.led_state(penalty_pos) is True

    # Release button -> WINNER sees both released -> WINNER_DISPLAY_DELAY
    if early_side == "left":
        sim.release_left()
    else:
        sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Advance past the 200ms display delay -> WINNER_WAIT_RESTART
    sim.advance_millis(201)
    sim.loop()

    # Press BOTH buttons -> WAIT_RELEASE -> release -> LIGHTING_UP, column 1 on
    start_sequence(sim)

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def _is_winner_display(sim: F1Sim, winner_row: str = "bottom") -> bool:
    """Check that we're still showing winner state (not restarted into LIGHTING_UP).

    The winner row blinks (250ms on / 250ms off), so at any given tick
    either the full row is on or everything is off.  The key invariant:
    the *other* row must always be completely off (no column-1 pattern).
    """
    top, bottom = sim.top_row(), sim.bottom_row()
    if winner_row == "bottom":
        # Top row must be all-off (no LIGHTING_UP column pattern)
        return top == [False, False, False, False, False]
    else:
        return bottom == [False, False, False, False, False]


def test_winner_display_not_skippable_before_200ms(sim_at_winner_display: F1Sim):
    """Winner display must persist for at least 200ms — button presses before that are ignored."""
    sim = sim_at_winner_display

    # Confirm right player's row is in winner display (blink on-phase right after entering state)
    assert _is_winner_display(sim, "bottom")

    # Spam both buttons at 40ms intervals — 4 × 40ms = 160ms total, still under the 200ms guard
    for elapsed_ms in (40, 80, 120, 160):
        sim.advance_millis(40)
        sim.press_both()
        sim.loop()
        assert _is_winner_display(sim, "bottom"), (
            f"Winner display state exited at {elapsed_ms}ms (before 200ms guard)"
        )

    sim.release_both()

    # Push just past 200ms -> WINNER_DISPLAY_DELAY ends -> WINNER_WAIT_RESTART
    sim.advance_millis(41)   # 160 + 41 = 201ms total
    sim.loop()

    # Press BOTH buttons -> WAIT_RELEASE -> release -> LIGHTING_UP, column 1 on
    start_sequence(sim)

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def test_full_f1_sequence(sim: F1Sim):
    """Walk through a complete game: IDLE -> LIGHTING_UP -> ALL_ON -> BLACKOUT -> right player wins."""

    # After setup: all LEDs off (IDLE state)
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Start sequence (press both, release both, column 1 on)
    sim.advance_millis(100)
    start_sequence(sim)

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }

    # Column 2 lights up after 1s
    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_states() == {
        1: True, 2: True, 3: False, 4: False, 5: False,
        6: True, 7: True, 8: False, 9: False, 10: False,
    }

    # Columns 3-5 light up at 1s intervals
    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_states() == {
        1: True, 2: True, 3: True, 4: False, 5: False,
        6: True, 7: True, 8: True, 9: False, 10: False,
    }

    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_states() == {
        1: True, 2: True, 3: True, 4: True, 5: False,
        6: True, 7: True, 8: True, 9: True, 10: False,
    }

    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_states() == {
        1: True, 2: True, 3: True, 4: True, 5: True,
        6: True, 7: True, 8: True, 9: True, 10: True,
    }

    # Transition to ALL_ON after 5s total
    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_states() == {i: True for i in range(1, 11)}

    # Advance past random delay (max 3s) -> BLACKOUT
    sim.advance_millis(4000)
    sim.loop()
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Right player presses -> RIGHT PLAYER WINS (bottom row on)
    sim.advance_millis(15)
    sim.press_right()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    assert sim.top_row() == [False, False, False, False, False]
    assert sim.bottom_row() == [True, True, True, True, True]

    # Release right button -> WINNER sees both released -> WINNER_DISPLAY_DELAY
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Advance past the 200ms display delay -> WINNER_WAIT_RESTART
    sim.advance_millis(201)
    sim.loop()

    # Restart: press both -> WAIT_RELEASE -> release -> LIGHTING_UP, column 1 on
    start_sequence(sim)

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def test_tie_both_buttons_pressed_simultaneously(sim: F1Sim):
    """When both players press at the same time after BLACKOUT, it's a tie — both rows blink."""

    # Start sequence (press both, release both, column 1 on)
    sim.advance_millis(100)
    start_sequence(sim)

    # Advance through LIGHTING_UP (5 columns × 1s)
    for _ in range(5):
        sim.advance_millis(1000)
        sim.loop()

    # Transition to ALL_ON
    sim.advance_millis(1000)
    sim.loop()

    # Advance past max random delay -> BLACKOUT
    sim.advance_millis(3001)
    sim.loop()
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Both players press at the same time -> TIE
    sim.advance_millis(15)
    sim.press_both()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    # On a blink-on phase, both rows should be on
    # Align to a known blink-on phase (millis / 250 is even)
    current = sim.get_millis()
    phase = (current // 250) % 2
    if phase != 0:
        # Advance to next even phase
        sim.advance_millis(250 - (current % 250))
        sim.loop()

    assert sim.top_row() == [True, True, True, True, True]
    assert sim.bottom_row() == [True, True, True, True, True]

    # On a blink-off phase, both rows should be off
    sim.advance_millis(250)
    sim.loop()

    assert sim.top_row() == [False, False, False, False, False]
    assert sim.bottom_row() == [False, False, False, False, False]


def test_winner_blink_stops_after_2s(sim_at_winner_display: F1Sim):
    """Winner row blinks for 2 seconds then all LEDs turn off."""
    sim = sim_at_winner_display

    # Confirm we're still in winner display (blink active)
    assert _is_winner_display(sim, "bottom")

    # Advance to just before 2s — blink should still be active
    sim.advance_millis(1900)
    sim.loop()
    assert _is_winner_display(sim, "bottom")

    # Advance past 2s — all LEDs must be off
    sim.advance_millis(200)
    sim.loop()

    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Further loop ticks should keep LEDs off
    sim.advance_millis(100)
    sim.loop()

    assert sim.led_states() == {i: False for i in range(1, 11)}
