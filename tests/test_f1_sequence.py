"""Test the full F1 start-light game sequence via the desktop simulation."""

from f1_sim import F1Sim


def test_full_f1_sequence(sim: F1Sim):
    """Walk through a complete game: IDLE -> LIGHTING_UP -> ALL_ON -> BLACKOUT -> right player wins."""

    # After setup: all LEDs off (IDLE state)
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Press left button to start the sequence
    sim.advance_millis(100)
    sim.press_left()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }

    # Release buttons so early-start detection can arm
    sim.release_left()
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
