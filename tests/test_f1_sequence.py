"""Test the F1 start-light game sequence via the desktop simulation.

State machine: IDLE → READY → LIGHTING → WAIT_GO → RACE → RESULT
                                ↘ JUMP_START ↗       ↗
"""

import pytest
from f1_sim import F1Sim


def start_round(sim: F1Sim):
    """Both players press → enter READY → both ready → LIGHTING starts.

    After returning, the sim is in LIGHTING with L1+L6 on and waitRelease cleared.
    """
    sim.press_a()
    sim.advance_millis(25)
    sim.loop()
    sim.press_b()
    sim.advance_millis(25)
    sim.loop()
    # Release both so waitRelease clears
    sim.release_both()
    sim.advance_millis(25)
    sim.loop()


def test_idle_all_leds_off(sim: F1Sim):
    """After setup: all LEDs off (IDLE state)."""
    assert sim.led_states() == {i: False for i in range(1, 11)}


def test_start_lighting_sequence(sim: F1Sim):
    """Both players press → READY → both ready → first LED pair turns on."""
    sim.advance_millis(100)
    start_round(sim)

    # First pair (L1 + L6) should be on
    assert sim.led_state(1) is True
    assert sim.led_state(6) is True
    # Others off
    for pos in (2, 3, 4, 5, 7, 8, 9, 10):
        assert sim.led_state(pos) is False, f"L{pos} should be off"


def test_lighting_sequence_pairs(sim: F1Sim):
    """LED pairs light up during LIGHTING: first gap 1000ms, then 800ms each."""
    sim.advance_millis(100)
    start_round(sim)

    # After start_round, L1+L6 are on (litCount=1)
    # Pair 2 fires at stateTimer = start + 1000ms
    sim.advance_millis(1000)
    sim.loop()
    assert sim.led_state(2) is True
    assert sim.led_state(7) is True

    # Pairs 3-5 at 800ms intervals
    sim.advance_millis(800)
    sim.loop()
    assert sim.led_state(3) is True
    assert sim.led_state(8) is True

    sim.advance_millis(800)
    sim.loop()
    assert sim.led_state(4) is True
    assert sim.led_state(9) is True

    sim.advance_millis(800)
    sim.loop()
    assert sim.led_state(5) is True
    assert sim.led_state(10) is True

    # All 10 LEDs should now be on
    assert sim.led_states() == {i: True for i in range(1, 11)}


def test_lights_out_race(sim: F1Sim):
    """After all pairs lit and random delay, lights go out (RACE state)."""
    sim.advance_millis(100)
    start_round(sim)

    # Advance through all pairs: 1000 + 3*800 = 3400ms
    sim.advance_millis(1000)
    sim.loop()
    for _ in range(3):
        sim.advance_millis(800)
        sim.loop()

    # All LEDs on
    assert sim.led_states() == {i: True for i in range(1, 11)}

    # Advance past max random delay (1s + 3s = 4s max) with loop calls
    for _ in range(25):
        sim.advance_millis(200)
        sim.loop()

    # All LEDs should be off (RACE)
    assert sim.led_states() == {i: False for i in range(1, 11)}


def test_player_a_wins(sim_at_race: F1Sim):
    """Player A presses first after lights out → A wins, top row LEDs on."""
    sim = sim_at_race

    sim.press_a()
    sim.advance_millis(25)
    sim.loop()
    # Allow the 100ms grace window to pass
    sim.advance_millis(120)
    sim.loop()

    # Player A wins: L1-L5 should be on (A's row)
    for pos in range(1, 6):
        assert sim.led_state(pos) is True, f"L{pos} should be on (Player A wins)"


def test_player_b_wins(sim_at_race: F1Sim):
    """Player B presses first after lights out → B wins, bottom row LEDs on."""
    sim = sim_at_race

    sim.press_b()
    sim.advance_millis(25)
    sim.loop()
    sim.advance_millis(120)
    sim.loop()

    # Player B wins: L6-L10 should be on
    for pos in range(6, 11):
        assert sim.led_state(pos) is True, f"L{pos} should be on (Player B wins)"


def test_tie_both_press(sim_at_race: F1Sim):
    """Both players press at the same time → tie, all LEDs on."""
    sim = sim_at_race

    sim.press_both()
    sim.advance_millis(25)
    sim.loop()
    sim.advance_millis(120)
    sim.loop()

    # Tie: all LEDs on
    assert sim.led_states() == {i: True for i in range(1, 11)}


@pytest.mark.parametrize("jump_side", ["a", "b"])
def test_jump_start_during_lighting(sim: F1Sim, jump_side):
    """Pressing during LIGHTING is a jump start — the other player wins."""
    sim.advance_millis(100)
    start_round(sim)

    # We're in LIGHTING with L1+L6 on
    assert sim.led_state(1) is True
    assert sim.led_state(6) is True

    # Jump start
    if jump_side == "a":
        sim.press_a()
    else:
        sim.press_b()
    sim.advance_millis(25)
    sim.loop()

    if jump_side == "a":
        # Player A jumped — B wins: L6-L10 on
        for pos in range(6, 11):
            assert sim.led_state(pos) is True, f"L{pos} should be on (B wins after A jump start)"
        # A's LEDs off
        for pos in range(1, 6):
            assert sim.led_state(pos) is False, f"L{pos} should be off"
    else:
        # Player B jumped — A wins: L1-L5 on
        for pos in range(1, 6):
            assert sim.led_state(pos) is True, f"L{pos} should be on (A wins after B jump start)"
        for pos in range(6, 11):
            assert sim.led_state(pos) is False, f"L{pos} should be off"


@pytest.mark.parametrize("jump_side", ["a", "b"])
def test_jump_start_during_wait_go(sim: F1Sim, jump_side):
    """Pressing during WAIT_GO (all lights on, waiting for blackout) is a jump start."""
    sim.advance_millis(100)
    start_round(sim)

    # Advance through all pairs: 1000 + 3*800 = 3400ms
    sim.advance_millis(1000)
    sim.loop()
    for _ in range(3):
        sim.advance_millis(800)
        sim.loop()

    # All on, now in WAIT_GO
    assert sim.led_states() == {i: True for i in range(1, 11)}

    # Jump start
    if jump_side == "a":
        sim.press_a()
    else:
        sim.press_b()
    sim.advance_millis(25)
    sim.loop()

    if jump_side == "a":
        for pos in range(6, 11):
            assert sim.led_state(pos) is True, f"L{pos} should be on (B wins)"
    else:
        for pos in range(1, 6):
            assert sim.led_state(pos) is True, f"L{pos} should be on (A wins)"


def test_result_restart(sim_at_race: F1Sim):
    """After RESULT, pressing a button enters READY for the next round."""
    sim = sim_at_race

    # Player A wins
    sim.press_a()
    sim.advance_millis(150)
    sim.loop()

    # Release button (waitRelease needs release first)
    sim.release_a()
    sim.advance_millis(25)
    sim.loop()

    # Now press A again to enter READY
    sim.press_a()
    sim.advance_millis(25)
    sim.loop()

    # Press B to make both ready → LIGHTING starts
    sim.press_b()
    sim.advance_millis(25)
    sim.loop()

    # Release both
    sim.release_both()
    sim.advance_millis(25)
    sim.loop()

    # First pair should be on (new round started)
    assert sim.led_state(1) is True
    assert sim.led_state(6) is True


def test_race_timeout(sim_at_race: F1Sim):
    """If nobody presses within 3 seconds, RACE times out → RESULT."""
    sim = sim_at_race

    # Wait 3+ seconds with loop calls
    for _ in range(16):
        sim.advance_millis(200)
        sim.loop()

    # All LEDs should be off (timeout → RESULT with dashes on display)
    assert sim.led_states() == {i: False for i in range(1, 11)}


def test_jump_start_both(sim: F1Sim):
    """Both players jump start simultaneously — both get dashes, no winner LEDs."""
    sim.advance_millis(100)
    start_round(sim)

    # Both jump during LIGHTING
    sim.press_both()
    sim.advance_millis(25)
    sim.loop()

    # No winner LEDs (both jumped)
    assert sim.led_states() == {i: False for i in range(1, 11)}
