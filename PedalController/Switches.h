#pragma once

#include <stdint.h>

namespace Switches {

enum class Event : uint8_t { Press, Release };

using Handler = void (*)(uint8_t sw, Event e);

void begin(Handler h);
void scan();

uint32_t stableState();

}
