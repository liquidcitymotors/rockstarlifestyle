#include "Expression.h"
#include "Config.h"

#include <Arduino.h>

// EXP1 -> 7-bit value: hardware averaging, an IIR low-pass, then a
// quantizer with hysteresis so the output only moves once the filtered
// reading is clearly past the edge of the current step.

namespace Expression {

namespace {

constexpr uint32_t PERIOD_MS = 2;

// 12-bit counts. Dead zones at both ends so 0 and 127 are always reachable
// even if the pedal's pot doesn't quite reach the rails.
constexpr int32_t LO   = 60;
constexpr int32_t HI   = 4035;
constexpr int32_t SPAN = HI - LO;   // ~31 counts per 7-bit step
constexpr int32_t HYST = 8;         // counts past a step edge before moving

constexpr uint8_t FILT_SHIFT = 3;   // IIR weight 1/8: ~16 ms time constant at 2 ms
constexpr uint8_t FRAC       = 4;   // filter state kept in 1/16 counts

int32_t  g_filt  = -1;
int      g_value = -1;
uint32_t g_last  = 0;

// Filtered reading (in 1/16 counts) where step v begins.
inline int32_t edge(int32_t v) { return (LO << FRAC) + v * (SPAN << FRAC) / 128; }

}

void begin() {
    analogReadResolution(12);
    analogReadAveraging(16);
}

bool poll() {
    uint32_t now = millis();
    if (now - g_last < PERIOD_MS) return false;
    g_last = now;

    int32_t raw = (int32_t)analogRead(Config::PIN_EXP1) << FRAC;
    if (g_filt < 0) g_filt = raw;
    else            g_filt += (raw - g_filt) >> FILT_SHIFT;

    int32_t q = (g_filt - (LO << FRAC)) * 128 / (SPAN << FRAC);
    if (q < 0)   q = 0;
    if (q > 127) q = 127;

    if (q == g_value) return false;

    if (g_value >= 0) {
        constexpr int32_t h = HYST << FRAC;
        bool above = g_filt > edge(g_value + 1) + h;
        bool below = g_filt < edge(g_value) - h;
        if (!above && !below) return false;
    }

    g_value = q;
    return true;
}

int value() { return g_value; }

}
