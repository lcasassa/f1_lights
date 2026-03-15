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


@pytest.fixture()
def sim_at_winner_display(sim: F1Sim) -> F1Sim:
    """Drive the sim from IDLE to WINNER_DISPLAY_DELAY with right player winning."""
    # Start sequence: press both -> WAIT_RELEASE -> release -> LIGHTING_UP
    sim.advance_millis(100)
    sim.press_both()
    sim.advance_millis(15)
    sim.loop()
    sim.release_both()
    sim.advance_millis(15)
    sim.loop()
    sim.advance_millis(1)
    sim.loop()

    # Tick through LIGHTING_UP: 5 columns × 1000ms each
    for _ in range(5):
        sim.advance_millis(1000)
        sim.loop()

    # Tick into ALL_ON
    sim.advance_millis(1000)
    sim.loop()

    # Advance past max random delay (3000ms) -> BLACKOUT
    sim.advance_millis(3001)
    sim.loop()

    # Right player wins
    sim.advance_millis(15)
    sim.press_right()
    sim.loop()
    sim.advance_millis(15)
    sim.loop()

    # Wait 250ms before WINNER state starts listening for button releases
    sim.advance_millis(250)
    sim.loop()

    # Release right button -> WINNER sees both released -> WINNER_DISPLAY_DELAY
    sim.release_right()
    sim.advance_millis(15)
    sim.loop()

    return sim


