#include <Arduino.h>
#include <IntervalTimer.h>

#include "Config.h"
#include "Switches.h"
#include "MidiOut.h"
#include "Leds.h"
#include "Display.h"
#include "Expression.h"

// Controls, by footswitch position p = row * 6 + col (rows top to bottom):
//   rows 0-2 (p 0..17)  latching on/off, LED shows state, CC_BASE + p
//                       each also owns an expression CC, EXP_CC_BASE + p,
//                       driven by EXP1 while that switch has focus
//   row 3    (p 18..23) momentary, no LED, CC_BASE + p
//
// Toggle switch, press while off      -> on, and take focus
// Toggle switch, tap while on         -> off (sent on release)
// Toggle switch, hold while on        -> take focus, stays on
//
// Soft takeover: after focus moves, EXP1 doesn't change the focused CC until
// the pedal reaches (or passes) that CC's last value. While waiting, the
// focused LED blinks and the display's dot is lit. The display shows the
// focused CC's value scaled to 0..99.

namespace Switches { void dispatchPending(); }

static IntervalTimer g_scan_timer;

static constexpr uint8_t  CC_BASE     = 0;
static constexpr uint8_t  NUM_POS     = Config::SW_ROWS * Config::SW_COLS;
static constexpr uint8_t  NUM_TOGGLES = Config::GRID_ROWS * Config::GRID_COLS;
static constexpr uint8_t  EXP_CC_BASE = CC_BASE + NUM_POS;
static constexpr uint32_t HOLD_MS     = 400;
static constexpr uint32_t BLINK_MS    = 100;
static constexpr uint8_t  NO_POS      = 0xFF;

static uint8_t  g_pos_of_sw[Config::NUM_SWITCHES];

static bool     g_on[NUM_TOGGLES];
static uint8_t  g_exp_val[NUM_TOGGLES];
static uint32_t g_press_ms[NUM_TOGGLES];
static bool     g_hold_armed[NUM_TOGGLES];   // pressed while on: tap or hold?

static uint8_t  g_focus  = NO_POS;
static bool     g_caught = false;
static int8_t   g_side   = 0;                // pedal below (-1) / above (+1) target

static uint8_t ledOf(uint8_t p) {
    return Config::GRID_LED[p / Config::GRID_COLS][p % Config::GRID_COLS];
}

static void setToggleLed(uint8_t p, bool on) {
    uint8_t ch = ledOf(p);
    if (ch != Config::NO_LED) Leds::set(ch, on);
}

static void sendCc(uint8_t cc, uint8_t value) {
    Serial.print("CC "); Serial.print(cc); Serial.print(" = "); Serial.println(value);
    MidiOut::ccAllChannels(cc, value);
}

static void refreshDisplay() {
    if (g_focus == NO_POS) { Display::blank(); Display::setDot(false); return; }
    Display::showNumber((g_exp_val[g_focus] * 99 + 63) / 127);
    Display::setDot(!g_caught);
}

static int8_t sideOf(int pedal, uint8_t target) {
    return pedal < target ? -1 : pedal > target ? 1 : 0;
}

static void setFocus(uint8_t p) {
    if (g_focus != NO_POS && g_focus != p) setToggleLed(g_focus, g_on[g_focus]);
    g_focus  = p;
    g_side   = sideOf(Expression::value(), g_exp_val[p]);
    g_caught = Expression::value() >= 0 && g_side == 0;
    Serial.print("focus "); Serial.println(p);
    refreshDisplay();
}

static void setToggle(uint8_t p, bool on) {
    g_on[p] = on;
    setToggleLed(p, on);
    sendCc(CC_BASE + p, on ? 127 : 0);
}

static void onSwitchEvent(uint8_t sw, Switches::Event e) {
    uint8_t p = g_pos_of_sw[sw];
    if (p == NO_POS) return;
    bool press = e == Switches::Event::Press;

    if (p >= NUM_TOGGLES) {                         // bottom row: momentary
        sendCc(CC_BASE + p, press ? 127 : 0);
        return;
    }

    if (press) {
        if (!g_on[p]) {
            setToggle(p, true);
            setFocus(p);
        } else {
            g_hold_armed[p] = true;
            g_press_ms[p]   = millis();
        }
    } else if (g_hold_armed[p]) {                   // released before HOLD_MS: tap
        g_hold_armed[p] = false;
        setToggle(p, false);
    }
}

static void checkHolds() {
    uint32_t now = millis();
    for (uint8_t p = 0; p < NUM_TOGGLES; ++p) {
        if (g_hold_armed[p] && now - g_press_ms[p] >= HOLD_MS) {
            g_hold_armed[p] = false;
            setFocus(p);
        }
    }
}

static void onPedal() {
    if (g_focus == NO_POS) return;
    int v = Expression::value();

    if (!g_caught) {
        int8_t side = sideOf(v, g_exp_val[g_focus]);
        if (side != 0 && side == g_side) return;    // hasn't reached the old value yet
        g_caught = true;
    }

    if (g_exp_val[g_focus] == v) { refreshDisplay(); return; }
    g_exp_val[g_focus] = v;
    sendCc(EXP_CC_BASE + g_focus, v);
    refreshDisplay();
}

static void blinkFocus() {
    if (g_focus == NO_POS) return;
    if (g_caught) setToggleLed(g_focus, g_on[g_focus]);
    else          setToggleLed(g_focus, (millis() / BLINK_MS) & 1);
}

static void onScanTick() {
    Switches::scan();
}

void setup() {
    Serial.begin(115200);

    for (uint8_t sw = 0; sw < Config::NUM_SWITCHES; ++sw) g_pos_of_sw[sw] = NO_POS;
    for (uint8_t r = 0; r < Config::SW_ROWS; ++r)
        for (uint8_t c = 0; c < Config::SW_COLS; ++c)
            g_pos_of_sw[Config::GRID_SW[r][c]] = r * Config::SW_COLS + c;

    Leds::begin();
    Expression::begin();
    refreshDisplay();
    Switches::begin(onSwitchEvent);
    g_scan_timer.begin(onScanTick, Config::SCAN_PERIOD_US);
}

void loop() {
    Switches::dispatchPending();
    checkHolds();
    if (Expression::poll()) onPedal();
    blinkFocus();
    Leds::update();
}
