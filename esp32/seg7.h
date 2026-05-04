// Two physical 5-digit 7-seg modules wired to the same HT16K33. They share
// the same 5 COM lines (one digit-select per character cell) but use
// disjoint sets of ROW lines for their segments, so we can light any
// pattern on each independently — provided every write is masked so it
// only touches that display's bits.
//
//   kTop: A→ROW5, B→ROW9, C→ROW0, D→ROW10, E→ROW1, F→ROW8, G→ROW6, DP→ROW2
//   kBot: A→ROW3, B→ROW15, C→ROW11, D→ROW13, E→ROW12, F→ROW14, G→ROW4, DP→ROW7
#pragma once

#include <Arduino.h>

namespace seg7 {

struct Map {
  uint16_t A, B, C, D, E, F, G, DP;
  constexpr uint16_t mask() const { return A | B | C | D | E | F | G | DP; }
};

extern const Map kTop;
extern const Map kBot;

// Render right-aligned text. '.' chars fold into the previous glyph's DP,
// so "12.34" fits in 4 cells. Extra chars truncated, missing chars
// blank-padded on the LEFT.
void writeText(const Map &m, const char *s);

// Render a float using as many fractional digits as fit in the panel.
// '-' for negatives consumes one cell. Overflow shows "-----".
void printFloat(const Map &m, float value);

// Blank only display `m` (leaves the other display + RGB LEDs alone).
void clear(const Map &m);

}  // namespace seg7

