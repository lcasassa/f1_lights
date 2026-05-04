// Segment-bit scanner — bit-mapping debug aid for the HT16K33 backpack.
// Compiled in only when -DSEGMENT_SCAN=1 is set in the build env.
#pragma once

#if defined(SEGMENT_SCAN) && (SEGMENT_SCAN)

namespace segment_scan {

// Never returns. Pick mode with -DSEGMENT_SCAN_MODE=0 (sequential walk)
// or 1 (interactive binary search; default).
[[noreturn]] void run();

}  // namespace segment_scan

#endif

