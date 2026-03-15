import pytest
from f1_sim import F1Sim
@pytest.fixture()
def sim():
    """Provide a freshly-reset F1Sim instance for each test."""
    s = F1Sim()
    s.reset()
    s.set_millis(0)
    s.setup()
    return s
