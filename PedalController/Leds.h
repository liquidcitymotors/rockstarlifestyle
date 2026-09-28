#pragma once

#include <stdint.h>

namespace Leds {

void begin();
void set(uint8_t idx, bool on);
void setAll(uint32_t mask);
void update();

uint32_t state();

}
