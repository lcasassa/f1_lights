#!/usr/bin/env python3
"""Small harness to preview the blink timing used in src/main.cpp."""

from __future__ import annotations

BLINK_INTERVAL_MS = 500


def simulate_blink(total_ms: int = 3000) -> list[str]:
    state = "OFF"
    events: list[str] = [f"t=0ms -> {state}"]

    elapsed = BLINK_INTERVAL_MS
    while elapsed <= total_ms:
        state = "ON" if state == "OFF" else "OFF"
        events.append(f"t={elapsed}ms -> {state}")
        elapsed += BLINK_INTERVAL_MS

    return events


if __name__ == "__main__":
    print("Blink preview:")
    for line in simulate_blink():
        print(line)

