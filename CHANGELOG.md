# Changelog

All notable changes to the F1 Start Lights project are documented here.
Written for anyone following the project — no code knowledge required.

---

## 2026-03-15

- **Restart debounce delay** — After entering the WINNER state, restart-readiness tracking now waits 250 ms before monitoring button presses/releases. This prevents a natural button release (from the winning press) from being counted as a "ready" signal.
- **Web simulator** — Play in the browser with no install; the same C++ code runs via WebAssembly (`make web-dev` for dev, `make web` for production build).
- **Staggered restart fix** — Start sequence does not start if a player does not release the button after winning or losing, and expects the release to be the signal to start the sequence (the other player pressed and released). Forcing both players to press again.
- **Release before start** — The light sequence only begins after both players release their buttons, preventing accidental early starts from the starting press.
- **Winner blink auto-off** — The winning row blinks for 2 seconds then all LEDs turn off automatically.
- **Tie detection** — When both players press at exactly the same time, the game declares a tie and both rows blink together.
- **Desktop simulator** — Run the game on your computer with a visual LED display (`make display`); hold **1** for left player, **2** for right player.
- **Automated tests** — Test suite verifying the full sequence, early starts, 200 ms display guard, and ties (`make test`).
- **Both buttons to start** — Starting and restarting now requires both players to press at the same time.
- **Winner row blinks** — The winning row blinks on and off (4 times/sec) instead of staying solid.
- **Penalty LED stays solid** — On a false start, the guilty player's penalty LED stays permanently on while the winner row blinks.
- **Simulator timing fix** — The on-screen simulation no longer drifts behind real time.

---

## 2026-03-14

- **Initial F1 start-light game** — 10 LEDs in two rows of 5; columns light left-to-right (1 per second), random 1–3 s pause, then all out = GO.
- **Winner detection** — First player to press after blackout wins; their row lights up.
- **Early-start detection** — Pressing before blackout is a false start; the other player wins automatically with a penalty indicator LED.
- **200 ms display guard** — Winner result stays visible for at least 200 ms before a restart is accepted.
- **Pin 13 conflict fix** — Moved off pin 13 (board's built-in LED) to avoid ghost-lighting; random seed now uses analog pin A5.
