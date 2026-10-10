#pragma once

#include <stdint.h>

namespace Expression {

void begin();

// Call from loop(). Returns true when the 7-bit value changed.
bool poll();

// 0..127, or -1 before the first reading.
int value();

}
