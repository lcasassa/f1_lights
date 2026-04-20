import subprocess

import pytest
from f1_sim import F1Sim, _PROJECT_ROOT


@pytest.fixture(scope="session", autouse=True)
def build_sim_lib():
    """Run `make sim` before the test session — make decides if a rebuild is needed."""
    subprocess.check_call(["make", "sim"], cwd=_PROJECT_ROOT)


@pytest.fixture()
def sim():
    """Provide a freshly-reset F1Sim instance for each test."""
    s = F1Sim()
    s.reset()
    s.set_millis(0)
    s.setup()
    return s


def _drive_to_race(sim: F1Sim) -> F1Sim:
    """Drive sim from IDLE through to RACE (lights out).

    Uses generous timing to ensure we get past the random WAIT_GO delay,
    but stops as soon as all LEDs turn off (RACE state entered).
    """
    sim.advance_millis(100)
    # Press A to enter READY
    sim.press_a()
    sim.advance_millis(25)
    sim.loop()
    # Press B to make both ready → starts LIGHTING
    sim.press_b()
    sim.advance_millis(25)
    sim.loop()
    # Release both (waitRelease clears)
    sim.release_both()
    sim.advance_millis(25)
    sim.loop()

    # Advance through LIGHTING + WAIT_GO until lights go out.
    # We know at least 1 LED is on (L1+L6 after start), so wait for all off.
    all_off = {i: False for i in range(1, 11)}
    for _ in range(100):
        sim.advance_millis(100)
        sim.loop()
        if sim.led_states() == all_off:
            return sim

    raise AssertionError("Failed to reach RACE state (lights out) after 10 seconds")


@pytest.fixture()
def sim_at_race(sim: F1Sim) -> F1Sim:
    """Drive the sim from IDLE through to RACE (lights out, waiting for button press)."""
    return _drive_to_race(sim)
