========== LED MAPPING SUMMARY ==========
// index, byte, bit, label

// --- Copy-paste block ---
struct LedMap { uint8_t byteIdx; uint8_t bitIdx; const char* label; };
const LedMap ledMap[] = {
  {0, 0, "1DP"},
  {0, 1, "1D"},
  {0, 2, "5A"},
  {0, 3, "5B"},
  {0, 4, "5D"},
  {0, 5, "1A"},
  {0, 6, "1C"},
  {0, 7, "5G"},
  {1, 0, "1F"},
  {1, 1, "1B"},
  {1, 2, "1G"},
  {1, 3, "5E"},
  {1, 4, "5DP"},
  {1, 5, "5C"},
  {1, 6, "5F"},
  {2, 0, "2DP"},
  {2, 1, "2D"},
  {2, 2, "6A"},
  {2, 3, "6B"},
  {2, 4, "6D"},
  {2, 5, "2A"},
  {2, 6, "2C"},
  {2, 7, "6G"},
  {3, 0, "2F"},
  {3, 1, "2B"},
  {3, 2, "2G"},
  {3, 3, "6E"},
  {3, 4, "6DP"},
  {3, 5, "6C"},
  {3, 6, "6F"},
  {4, 0, "3DP"},
  {4, 1, "3D"},
  {4, 2, "7A"},
  {4, 3, "7B"},
  {4, 4, "7D"},
  {4, 5, "3A"},
  {4, 6, "3C"},
  {4, 7, "7G"},
  {5, 0, "3F"},
  {5, 1, "3B"},
  {5, 2, "3G"},
  {5, 3, "7E"},
  {5, 4, "7DP"},
  {5, 5, "7C"},
  {5, 6, "7F"},
  {6, 0, "L1"},
  {6, 1, "L8"},
  {6, 2, "L2"},
  {6, 3, "L6"},
  {6, 4, "8D"},
  {6, 5, "L9"},
  {6, 6, "L3"},
  {6, 7, "8G"},
  {7, 0, "L10"},
  {7, 1, "L5"},
  {7, 2, "L4"},
  {7, 3, "8E"},
  {7, 4, "8DP"},
  {7, 5, "8C"},
  {7, 6, "L7"}
};
constexpr uint8_t LED_MAP_SIZE = 60;
// --- End copy-paste block ---

// Simple table:
//  idx=0  byte=0  bit=0  -> 1DP
//  idx=1  byte=0  bit=1  -> 1D
//  idx=2  byte=0  bit=2  -> 5A
//  idx=3  byte=0  bit=3  -> 5B
//  idx=4  byte=0  bit=4  -> 5D
//  idx=5  byte=0  bit=5  -> 1A
//  idx=6  byte=0  bit=6  -> 1C
//  idx=7  byte=0  bit=7  -> 5G
//  idx=8  byte=1  bit=0  -> 1F
//  idx=9  byte=1  bit=1  -> 1B
//  idx=10  byte=1  bit=2  -> 1G
//  idx=11  byte=1  bit=3  -> 5E
//  idx=12  byte=1  bit=4  -> 5DP
//  idx=13  byte=1  bit=5  -> 5C
//  idx=14  byte=1  bit=6  -> 5F
//  idx=16  byte=2  bit=0  -> 2DP
//  idx=17  byte=2  bit=1  -> 2D
//  idx=18  byte=2  bit=2  -> 6A
//  idx=19  byte=2  bit=3  -> 6B
//  idx=20  byte=2  bit=4  -> 6D
//  idx=21  byte=2  bit=5  -> 2A
//  idx=22  byte=2  bit=6  -> 2C
//  idx=23  byte=2  bit=7  -> 6G
//  idx=24  byte=3  bit=0  -> 2F
//  idx=25  byte=3  bit=1  -> 2B
//  idx=26  byte=3  bit=2  -> 2G
//  idx=27  byte=3  bit=3  -> 6E
//  idx=28  byte=3  bit=4  -> 6DP
//  idx=29  byte=3  bit=5  -> 6C
//  idx=30  byte=3  bit=6  -> 6F
//  idx=32  byte=4  bit=0  -> 3DP
//  idx=33  byte=4  bit=1  -> 3D
//  idx=34  byte=4  bit=2  -> 7A
//  idx=35  byte=4  bit=3  -> 7B
//  idx=36  byte=4  bit=4  -> 7D
//  idx=37  byte=4  bit=5  -> 3A
//  idx=38  byte=4  bit=6  -> 3C
//  idx=39  byte=4  bit=7  -> 7G
//  idx=40  byte=5  bit=0  -> 3F
//  idx=41  byte=5  bit=1  -> 3B
//  idx=42  byte=5  bit=2  -> 3G
//  idx=43  byte=5  bit=3  -> 7E
//  idx=44  byte=5  bit=4  -> 7DP
//  idx=45  byte=5  bit=5  -> 7C
//  idx=46  byte=5  bit=6  -> 7F
//  idx=48  byte=6  bit=0  -> L1
//  idx=49  byte=6  bit=1  -> L8
//  idx=50  byte=6  bit=2  -> L2
//  idx=51  byte=6  bit=3  -> L6
//  idx=52  byte=6  bit=4  -> 8D
//  idx=53  byte=6  bit=5  -> L9
//  idx=54  byte=6  bit=6  -> L3
//  idx=55  byte=6  bit=7  -> 8G
//  idx=56  byte=7  bit=0  -> L10
//  idx=57  byte=7  bit=1  -> L5
//  idx=58  byte=7  bit=2  -> L4
//  idx=59  byte=7  bit=3  -> 8E
//  idx=60  byte=7  bit=4  -> 8DP
//  idx=61  byte=7  bit=5  -> 8C
//  idx=62  byte=7  bit=6  -> L7
=========================================