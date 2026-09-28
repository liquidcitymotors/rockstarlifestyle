#include "MidiOut.h"

#include <Arduino.h>

namespace MidiOut {

void ccAllChannels(uint8_t cc, uint8_t value) {
    for (uint8_t ch = 1; ch <= 16; ++ch) usbMIDI.sendControlChange(cc, value, ch);
    usbMIDI.send_now();
}

}
