#include <Arduino.h>
#include <IntervalTimer.h>

#include "Config.h"
#include "Switches.h"
#include "MidiOut.h"
#include "Leds.h"

namespace Switches { void dispatchPending(); }

static IntervalTimer g_scan_timer;

static constexpr uint8_t CC_BASE = 20;

static uint32_t g_toggle_state = 0;

static void onScanTick() {
    Switches::scan();
}

static void onSwitchEvent(uint8_t sw, Switches::Event e) {
    if (e != Switches::Event::Press) return;

    g_toggle_state ^= (1UL << sw);
    bool on        = (g_toggle_state >> sw) & 1;
    uint8_t cc     = CC_BASE + sw;
    uint8_t value  = on ? 127 : 0;

    Leds::set(sw, on);

    Serial.print("SW ");
    Serial.print(sw);
    Serial.print(" CC ");
    Serial.print(cc);
    Serial.print(" = ");
    Serial.println(value);

    MidiOut::ccAllChannels(cc, value);
}

void setup() {
    Serial.begin(115200);
    Leds::begin();
    Switches::begin(onSwitchEvent);
    g_scan_timer.begin(onScanTick, Config::SCAN_PERIOD_US);
}

void loop() {
    Switches::dispatchPending();
    Leds::update();
}
