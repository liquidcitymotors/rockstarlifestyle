#include "Switches.h"
#include "Config.h"

#include <Arduino.h>

namespace Switches {

namespace {

Handler g_handler = nullptr;

volatile uint32_t g_raw_state    = 0;
volatile uint32_t g_stable_state = 0;

uint8_t g_counters[Config::NUM_SWITCHES] = {0};

uint32_t g_pending_events = 0;
uint32_t g_pending_edge   = 0;

inline void selectChannel(uint8_t ch) {
    digitalWriteFast(Config::PIN_MUX_SEL_A, (ch >> 0) & 1);
    digitalWriteFast(Config::PIN_MUX_SEL_B, (ch >> 1) & 1);
    digitalWriteFast(Config::PIN_MUX_SEL_C, (ch >> 2) & 1);
}

}

void begin(Handler h) {
    g_handler = h;

    pinMode(Config::PIN_MUX_SEL_A, OUTPUT);
    pinMode(Config::PIN_MUX_SEL_B, OUTPUT);
    pinMode(Config::PIN_MUX_SEL_C, OUTPUT);

    for (uint8_t m = 0; m < Config::NUM_MUXES; ++m) {
        pinMode(Config::PIN_MUX_Z[m], INPUT_PULLUP);
    }

    selectChannel(0);
}

void scan() {
    uint32_t raw = 0;

    for (uint8_t ch = 0; ch < Config::CHANS_PER_MUX; ++ch) {
        selectChannel(ch);
        delayMicroseconds(Config::MUX_SETTLE_US);

        for (uint8_t m = 0; m < Config::NUM_MUXES; ++m) {
            bool pressed = !digitalReadFast(Config::PIN_MUX_Z[m]);
            uint8_t sw = m * Config::CHANS_PER_MUX + ch;
            if (pressed) raw |= (1UL << sw);
        }
    }

    g_raw_state = raw;

    uint32_t stable = g_stable_state;
    uint32_t events = 0;

    for (uint8_t sw = 0; sw < Config::NUM_SWITCHES; ++sw) {
        bool r = (raw    >> sw) & 1;
        bool s = (stable >> sw) & 1;

        if (r == s) {
            g_counters[sw] = 0;
            continue;
        }

        if (++g_counters[sw] >= Config::DEBOUNCE_N) {
            g_counters[sw] = 0;
            stable ^= (1UL << sw);
            events |= (1UL << sw);
        }
    }

    if (events) {
        g_stable_state = stable;

        noInterrupts();
        g_pending_events |= events;
        g_pending_edge    = (g_pending_edge & ~events) | (stable & events);
        interrupts();
    }
}

uint32_t stableState() { return g_stable_state; }

}

namespace Switches {

void dispatchPending();

void dispatchPending() {
    noInterrupts();
    uint32_t events = g_pending_events;
    uint32_t edge   = g_pending_edge;
    g_pending_events = 0;
    interrupts();

    if (!events || !g_handler) return;

    for (uint8_t sw = 0; sw < Config::NUM_SWITCHES; ++sw) {
        if (!((events >> sw) & 1)) continue;
        Event e = ((edge >> sw) & 1) ? Event::Press : Event::Release;
        g_handler(sw, e);
    }
}

}
