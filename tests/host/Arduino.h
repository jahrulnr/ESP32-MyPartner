#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>

using std::max;
using std::min;
inline uint32_t hostMillis = 0;
inline unsigned long millis() { return hostMillis; }
inline long random(long low, long high) { return low + std::rand() % (high - low); }
