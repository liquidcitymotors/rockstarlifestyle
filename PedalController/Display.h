#pragma once

#include <stdint.h>

namespace Display {

void blank();
void showNumber(uint8_t n);   // 0..99, no leading zero
void setDot(bool on);         // right digit's decimal point

}
