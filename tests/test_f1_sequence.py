"""Test the full F1 start-light game sequence via the desktop simulation."""

import pytest
from f1_sim import F1Sim


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
    # Start sequence with both buttons
    sim.advance_millis(100)
    sim.press_both()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    # Release both so early-start detection arms
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()

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

    # Release left/right button -> WINNER state sees both released -> WINNER_DISPLAY_DELAY
    if early_side == "left":
        sim.release_left()
    else:
        sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Advance past the 200ms display delay -> WINNER_WAIT_RESTART
    sim.advance_millis(201)
    sim.loop()

    # Press BOTH buttons -> restart -> LIGHTING_UP, column 1 turns on immediately
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # WINNER_WAIT_RESTART: detects both pressed, transitions to LIGHTING_UP
    sim.advance_millis(1)
    sim.loop()   # LIGHTING_UP: column 1 lights up

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def test_winner_display_not_skippable_before_200ms(sim_at_winner_display: F1Sim):
    """Winner display must persist for at least 200ms — button presses before that are ignored."""
    sim = sim_at_winner_display

    # Confirm right player's row is still on
    assert sim.bottom_row() == [True, True, True, True, True]

    # Spam both buttons at 40ms intervals — 4 × 40ms = 160ms total, still under the 200ms guard
    for elapsed_ms in (40, 80, 120, 160):
        sim.advance_millis(40)
        sim.press_both()
        sim.loop()
        assert sim.bottom_row() == [True, True, True, True, True], (
            f"Winner display disappeared at {elapsed_ms}ms (before 200ms guard)"
        )

    sim.release_both()

    # Push just past 200ms -> WINNER_DISPLAY_DELAY ends -> WINNER_WAIT_RESTART
    sim.advance_millis(41)   # 160 + 41 = 201ms total
    sim.loop()

    # Pressing both now must restart the sequence
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # transitions to LIGHTING_UP
    sim.advance_millis(1)
    sim.loop()   # column 1 lights up

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def test_full_f1_sequence(sim: F1Sim):
    """Walk through a complete game: IDLE -> LIGHTING_UP -> ALL_ON -> BLACKOUT -> right player wins."""

    # After setup: all LEDs off (IDLE state)
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Press BOTH buttons to start the sequence
    sim.advance_millis(100)
    sim.press_both()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }

    # Release both buttons so early-start detection can arm
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()

    # Column 1 still on
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

    # Release right button -> WINNER state sees both released -> WINNER_DISPLAY_DELAY
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Advance past the 200ms display delay -> WINNER_WAIT_RESTART
    sim.advance_millis(201)
    sim.loop()

    # Press BOTH buttons -> restart -> LIGHTING_UP, column 1 turns on immediately
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # WINNER_WAIT_RESTART: detects both pressed, transitions to LIGHTING_UP
    sim.advance_millis(1)
    sim.loop()   # LIGHTING_UP: column 1 lights up

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


