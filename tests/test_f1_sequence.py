    sim.loop()   # both ready -> LIGHTING_UP
        return bottom == [False, False, False, False, False]
        # Top row must be all-off (no LIGHTING_UP column pattern)
        return top == [False, False, False, False, False]
"""Test the full F1 start-light game sequence via the desktop simulation."""

import pytest
from f1_sim import F1Sim


def start_sequence(sim: F1Sim):
    """Press both buttons, release both, wait for LIGHTING_UP to begin (from IDLE)."""
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # IDLE -> WAIT_RELEASE
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()   # WAIT_RELEASE -> LIGHTING_UP, column 1 lights up
    sim.advance_millis(1)
    sim.loop()   # LIGHTING_UP: column 1 on


def restart_sequence(sim: F1Sim):
    """Both players press-and-release to signal ready (from winner states)."""
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()   # both seen pressed
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()   # both ready -> LIGHTING_UP
    sim.advance_millis(1)
    sim.loop()   # column 1 on


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

    # Wait 250ms before WINNER state starts listening for button releases
    sim.advance_millis(250)
    sim.loop()

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

    # Both players press-and-release -> ready -> LIGHTING_UP, column 1 on
    restart_sequence(sim)

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def _is_winner_display(sim: F1Sim, winner_row: str = "bottom") -> bool:
    positions 2-5 of the *other* row must always be off (no column-2+
    LIGHTING_UP pattern).  Position 1 may be on due to the ready indicator.

    The winner row blinks (250ms on / 250ms off), so at any given tick
    either the full row is on or everything is off.  The key invariant:
        # Top row positions 2-5 must be off (pos 1 may be a ready indicator)
        return top[1:] == [False, False, False, False]
    top, bottom = sim.top_row(), sim.bottom_row()
        # Bottom row positions 7-10 must be off (pos 6 may be a ready indicator)
        return bottom[1:] == [False, False, False, False]
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
    sim.advance_millis(15)
    sim.loop()

    # Still under 200ms — should still be in winner display
    assert _is_winner_display(sim, "bottom")

    # Push past 200ms — both players already pressed-and-released, so sequence starts automatically
    sim.advance_millis(30)   # 160 + 15 + 30 = 205ms total
    sim.loop()
    sim.advance_millis(1)
    sim.loop()

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

    # Wait 250ms before WINNER state starts listening for button releases
    sim.advance_millis(250)
    sim.loop()

    # Release right button -> WINNER sees both released -> WINNER_DISPLAY_DELAY
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Advance past the 200ms display delay -> WINNER_WAIT_RESTART
    sim.advance_millis(201)
    sim.loop()

    # Restart: both players press-and-release -> ready -> LIGHTING_UP, column 1 on
    restart_sequence(sim)

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


def test_staggered_restart_after_winner(sim: F1Sim):
    """One player can press-and-release to signal ready while the winner still holds their button.
    Once the winner also releases, the sequence starts without needing both to press again.
    """
    # Start sequence
    sim.advance_millis(100)
    start_sequence(sim)

    # Advance through full sequence to BLACKOUT
    for _ in range(5):
        sim.advance_millis(1000)
        sim.loop()
    sim.advance_millis(1000)
    sim.loop()
    sim.advance_millis(3001)
    sim.loop()

    # Right player wins (and keeps holding button 2)
    sim.advance_millis(15)
    sim.press_right()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()
    assert sim.bottom_row() == [True, True, True, True, True]

    # Wait 250ms before WINNER state starts listening for restart readiness
    sim.advance_millis(250)
    sim.loop()

    # Left player presses and releases (signals ready) while right still holds
    sim.advance_millis(15)
    sim.press_left()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    # Both buttons released -> WINNER_DISPLAY_DELAY (200ms guard)
    sim.loop()
    # Left is now ready, right is still holding
    sim.loop()

    # Advance past the 200ms display guard; both already ready -> LIGHTING_UP
    sim.advance_millis(201)
    sim.loop()
    # Right player finally releases — right is now ready too (press already seen, now released)
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()   # both ready -> LIGHTING_UP
    sim.advance_millis(1)
    sim.loop()   # column 1 on

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def _drive_to_winner_wait_restart(sim: F1Sim) -> F1Sim:
    """Drive sim from IDLE through a full game to WINNER_WAIT_RESTART (right player wins).

    NOTE: The right player's winning press-release cycle counts as a ready
    signal, so right is already "ready" when we reach WINNER_WAIT_RESTART.
    """
    sim.advance_millis(100)
    start_sequence(sim)

    # Advance through LIGHTING_UP + ALL_ON + BLACKOUT
    for _ in range(5):
        sim.advance_millis(1000)
        sim.loop()
    sim.advance_millis(1000)
    sim.loop()
    sim.advance_millis(3001)
    sim.loop()

    # Right player wins
    sim.advance_millis(15)
    sim.press_right()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    # Wait 250ms debounce, then release -> WINNER_DISPLAY_DELAY -> WINNER_WAIT_RESTART
    sim.advance_millis(250)
    sim.loop()
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()
    sim.advance_millis(201)
    sim.loop()

    return sim


def test_ready_timeout_expires_after_2s(sim: F1Sim):
    """If one player signals ready and 2 seconds pass without the other, readiness expires.

    After _drive_to_winner_wait_restart, right is already ready (from the
    winning press-release).  We wait >2s so right's readiness expires,
    then left signals ready alone.  After 2s left's readiness also expires.
    Finally both signal together and the game restarts.
    """
    sim = _drive_to_winner_wait_restart(sim)
    # Right is already ready from the winning press-release.

    # Wait 2 seconds — right's readiness should expire (left never signalled)
    sim.advance_millis(2000)
    sim.loop()

    # Now left player signals ready
    sim.press_left()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    sim.loop()
    # Left is ready, right's readiness expired — should NOT restart

    assert _is_winner_display(sim, "bottom")

    # Wait 2 more seconds — left's readiness expires too
    sim.advance_millis(2000)
    sim.loop()

    # Now right signals ready
    sim.press_right()
    sim.advance_millis(15)
    sim.loop()
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    # Should NOT restart — left expired, only right is ready
    assert _is_winner_display(sim, "bottom")

    # Left signals again — now both are ready, restart happens
    sim.press_left()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    sim.loop()
    sim.advance_millis(1)
    sim.loop()

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
        6: True, 7: False, 8: False, 9: False, 10: False,
    }


def test_ready_within_2s_still_works(sim: F1Sim):
    """If both players signal ready within 2 seconds, the game restarts normally.

    After _drive_to_winner_wait_restart, right is already ready.  Left
    signals ready within 2s and the game restarts.
    """
    sim = _drive_to_winner_wait_restart(sim)
    # Right is already ready from winning press-release.

    # Left player signals ready within 2s
    sim.advance_millis(500)
    sim.loop()
    sim.press_left()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    sim.loop()
    sim.advance_millis(1)
    sim.loop()

    assert sim.led_states() == {
        1: True, 2: False, 3: False, 4: False, 5: False,
def test_ready_indicator_solid_for_ready_player(sim: F1Sim):
    """When one player signals ready (after blink ends), their first LED turns on solid."""
    sim = _drive_to_winner_wait_restart(sim)
    # Wait for winner blink AND right's initial readiness to expire
    sim.advance_millis(2100)
    sim.loop()

    # All LEDs should be off now (blink ended, readiness expired)
    assert sim.led_states() == {i: False for i in range(1, 11)}

    # Left player signals ready
    sim.press_left()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    sim.loop()

    # Left is ready -> POS-1 (left's first LED) must be solid ON
    assert sim.led_state(1) is True

    # The rest of both rows (except indicator positions) must be off
    for pos in (2, 3, 4, 5, 7, 8, 9, 10):
        assert sim.led_state(pos) is False, f"POS-{pos} should be off"


def test_ready_indicator_blinks_for_non_ready_player(sim: F1Sim):
    """When one player is ready, the other player's first LED blinks (500ms half-period)."""
    sim = _drive_to_winner_wait_restart(sim)
    # Wait for winner blink AND right's initial readiness to expire
    sim.advance_millis(2100)
    sim.loop()

    # Fresh ready signal from left player (now in clean post-blink window)
    sim.press_left()
    sim.advance_millis(15)
    sim.loop()
    sim.release_left()
    sim.advance_millis(15)
    sim.loop()
    # Left is ready; right is not. Ready indicator should be active.

    # Align to a 500ms boundary
    current = sim.get_millis()
    offset = 500 - (current % 500)
    if offset == 500:
        offset = 0
    if offset > 0:
        sim.advance_millis(offset)
        sim.loop()

    pos6_phase_a = sim.led_state(6)

    # Advance exactly 500ms to toggle the blink phase
    sim.advance_millis(500)
    sim.loop()

    pos6_phase_b = sim.led_state(6)

    # The two readings must differ (one on, one off = blink)
    assert pos6_phase_a != pos6_phase_b, (
        f"POS-6 should blink: was {pos6_phase_a} then {pos6_phase_b}"
    )

    # POS-1 stays solid throughout (left is ready)
    assert sim.led_state(1) is True


def test_ready_indicator_off_when_no_one_ready(sim: F1Sim):
    """After both players' readiness expires, all indicator LEDs are off."""
    sim = _drive_to_winner_wait_restart(sim)
    # Right is ready. Wait for right's readiness to expire (2s timeout).
    sim.advance_millis(2100)
    sim.loop()

    # Both expired — no indicators should be shown. All LEDs off
    # (winner blink also ended since > 2s from winner declared)
    assert sim.led_states() == {i: False for i in range(1, 11)}


        6: True, 7: False, 8: False, 9: False, 10: False,
    }
