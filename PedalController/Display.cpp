#include "Display.h"
#include "Config.h"
#include "Leds.h"

namespace Display {

namespace {

// Segment bits: bit 0 = a ... bit 6 = g.
constexpr uint8_t FONT[10] = {0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F};

void writeDigit(uint8_t d, uint8_t bits) {
    for (uint8_t s = 0; s < 7; ++s) {
        uint8_t ch = Config::DIGIT_SEG[d][s];
        if (ch != Config::NO_LED) Leds::set(ch, (bits >> s) & 1);
    }
}

}

void blank() {
    writeDigit(0, 0);
    writeDigit(1, 0);
}

void showNumber(uint8_t n) {
    if (n > 99) n = 99;
    writeDigit(0, n >= 10 ? FONT[n / 10] : 0);
    writeDigit(1, FONT[n % 10]);
}

void setDot(bool on) {
    uint8_t ch = Config::DIGIT_SEG[1][7];
    if (ch != Config::NO_LED) Leds::set(ch, on);
}

}
