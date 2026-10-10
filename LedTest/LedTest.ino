// LedTest — standalone bench harness for the LED driver board and the
// expression-pedal inputs.
//
// Self-contained on purpose: it re-declares the same pins and the same
// 74HC595 SPI driver as PedalController/Leds.cpp so you can flash it
// without disturbing the main firmware, but it exercises the identical
// shift-out path that will ship.
//
// Hardware: Teensy 3.2 -> M74HC595 daisy chain, LEDs common anode on 3.3V
//   DATA=pin11 (MOSI), CLK=pin13 (SCK), LATCH=pin10
//   EXP1=pin14 (A0), EXP2=pin15 (A1), pot wiper on tip, 3.3V/GND on ring/sleeve
//
// Build/flash:  ./flash.sh LedTest      Monitor:  ./serial.sh
//
// Mapping channels to lights: press n / p (or type a number + Enter) to
// light one channel at a time, then write down which physical light it is.
// The channel number is the bit index used by Leds::set() in the firmware.
//
// Serial menu @ 115200:
//   <number> Enter  solo that channel (only it lit)
//   n / p           next / previous channel (solo)
//   w               walk: auto-step through every channel, printing each
//   a / o           all on / all off
//   c               chase (single dot sweeping, looping)
//   e               expression pedal readout on/off
//   (always on)     footswitch presses print their switch number
//   .               stop any running animation, hold current state
//   ?               reprint this menu

#include <Arduino.h>
#include <SPI.h>

// ---- pins (mirror PedalController/Config.h) ----
constexpr uint8_t PIN_LED_LATCH = 10;
constexpr uint8_t PIN_LED_DATA  = 11;  // SPI0 MOSI
constexpr uint8_t PIN_LED_CLK   = 13;  // SPI0 SCK
constexpr uint8_t PIN_EXP[2]    = {14, 15};
constexpr uint8_t PIN_MUX_SEL[3] = {2, 3, 4};
constexpr uint8_t PIN_MUX_Z[3]   = {5, 6, 8};

// Sending bytes for chips that aren't fitted is harmless: they fall off
// the end of the chain, so raise this freely when chips are added.
constexpr uint8_t  NUM_CHIPS = 4;
constexpr uint8_t  NUM_CH    = NUM_CHIPS * 8;
constexpr uint32_t SPI_HZ    = 1000000;  // slow on purpose: long jumper wires to the 595s

// LEDs are common anode: anodes tied to V+, 595 output LOW = LED on.
constexpr bool LED_ACTIVE_LOW = true;

static uint64_t g_state = 0;

// ---- 74HC595 driver (mirrors Leds.cpp) ----
static void ledsFlush() {
    const uint64_t out = LED_ACTIVE_LOW ? ~g_state : g_state;

    SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    for (int8_t chip = NUM_CHIPS - 1; chip >= 0; --chip)
        SPI.transfer((out >> (chip * 8)) & 0xFF);
    SPI.endTransaction();

    digitalWriteFast(PIN_LED_LATCH, HIGH);
    delayMicroseconds(1);  // 595 needs a latch pulse wider than ~50 ns at 3.3V
    digitalWriteFast(PIN_LED_LATCH, LOW);
}

static void setAll(uint64_t mask) { g_state = mask; ledsFlush(); }
static uint64_t allMask() { return NUM_CH >= 64 ? ~0ULL : ((1ULL << NUM_CH) - 1); }

static void printChannel(uint8_t ch) {
    Serial.print(F("ch "));
    if (ch < 10) Serial.print(' ');
    Serial.print(ch);
    Serial.print(F("   chip ")); Serial.print(ch / 8 + 1);
    Serial.print(F(" Q")); Serial.print((char)('A' + ch % 8));
    Serial.print(F(" (pin ")); Serial.print(ch % 8 == 0 ? 15 : ch % 8);
    Serial.println(F(")"));
}

static uint8_t g_solo = NUM_CH - 1;  // so the first n lands on channel 0

static void solo(uint8_t ch) {
    g_solo = ch;
    setAll(1ULL << ch);
    printChannel(ch);
}

// ---- animation state ----
enum class Mode { Hold, Walk, Chase };
static Mode     g_mode     = Mode::Hold;
static uint32_t g_lastStep = 0;
static uint16_t g_stepMs   = 600;
static uint8_t  g_pos      = 0;

// ---- expression pedals ----
static bool     g_expOn      = false;  // unplugged jacks float and spam
static int      g_expLast[2] = {-100, -100};
static uint32_t g_expLastMs  = 0;
constexpr int      EXP_HYST   = 4;   // counts of 1023 before a change is printed
constexpr uint32_t EXP_PERIOD = 20;  // ms between reads

static void printExp(uint8_t i, int v) {
    constexpr uint8_t BAR = 32;
    uint8_t fill = (uint32_t)v * BAR / 1023;
    Serial.print(F("EXP")); Serial.print(i + 1);
    Serial.print(F(" "));
    if (v < 1000) Serial.print(' ');
    if (v < 100)  Serial.print(' ');
    if (v < 10)   Serial.print(' ');
    Serial.print(v);
    Serial.print(F("  cc ")); Serial.print(v >> 3);
    Serial.print(F("\t|"));
    for (uint8_t k = 0; k < BAR; ++k) Serial.print(k < fill ? '#' : ' ');
    Serial.println('|');
}

// Accumulate digits so multi-digit channels work; Enter (or any key) ends it.
static int g_numAcc = -1;

// ---- footswitches (mirrors Switches.cpp: 3 muxes x 8, active low) ----
constexpr uint8_t NUM_SW      = 24;
constexpr uint8_t DEBOUNCE_N  = 4;
static uint32_t g_swStable    = 0;
static uint8_t  g_swCount[NUM_SW];
static uint32_t g_swLastMs    = 0;

static void pollSwitches() {
    uint32_t now = millis();
    if (now == g_swLastMs) return;  // once per ms
    g_swLastMs = now;

    uint32_t raw = 0;
    for (uint8_t ch = 0; ch < 8; ++ch) {
        for (uint8_t b = 0; b < 3; ++b) digitalWriteFast(PIN_MUX_SEL[b], (ch >> b) & 1);
        delayMicroseconds(5);
        for (uint8_t m = 0; m < 3; ++m)
            if (!digitalReadFast(PIN_MUX_Z[m])) raw |= 1UL << (m * 8 + ch);
    }

    for (uint8_t sw = 0; sw < NUM_SW; ++sw) {
        bool r = (raw >> sw) & 1, st = (g_swStable >> sw) & 1;
        if (r == st) { g_swCount[sw] = 0; continue; }
        if (++g_swCount[sw] < DEBOUNCE_N) continue;
        g_swCount[sw] = 0;
        g_swStable ^= 1UL << sw;
        if (g_numAcc >= 0) continue;  // don't interleave with typing
        Serial.print(F("SW ")); if (sw < 10) Serial.print(' '); Serial.print(sw);
        Serial.print(F("   mux ")); Serial.print(sw / 8 + 1);
        Serial.print(F(" ch ")); Serial.print(sw % 8);
        Serial.println(r ? F("   down") : F("   up"));
    }
}

static void printMenu() {
    Serial.println();
    Serial.println(F("=== LED + expression bench ==="));
    Serial.print  (F("  <number> Enter  solo that channel (0..")); Serial.print(NUM_CH - 1); Serial.println(F(")"));
    Serial.println(F("  n / p           next / previous channel"));
    Serial.println(F("  w  walk (auto-step, prints each channel)"));
    Serial.println(F("  a  all on        o  all off"));
    Serial.println(F("  c  chase         .  stop/hold"));
    Serial.print  (F("  e  expression pedal readout (now "));
    Serial.print  (g_expOn ? F("on") : F("off")); Serial.println(F(")"));
    Serial.println(F("  ?  this menu"));
    Serial.println(F("  footswitches print their number when pressed"));
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED_LATCH, OUTPUT);
    digitalWriteFast(PIN_LED_LATCH, LOW);
    SPI.begin();
    setAll(0);

    for (uint8_t b = 0; b < 3; ++b) pinMode(PIN_MUX_SEL[b], OUTPUT);
    for (uint8_t m = 0; m < 3; ++m) pinMode(PIN_MUX_Z[m], INPUT_PULLUP);

    analogReadResolution(10);
    analogReadAveraging(16);

    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for host */ }
    printMenu();
    Serial.println(F("Tip: press n repeatedly and note which light each channel is."));
}


static void handleChar(char c) {
    if (c >= '0' && c <= '9') {
        g_numAcc = (g_numAcc < 0 ? 0 : g_numAcc) * 10 + (c - '0');
        Serial.print(c);  // echo, since screen doesn't
        return;
    }
    if (g_numAcc >= 0) {
        int ch = g_numAcc;
        g_numAcc = -1;
        Serial.println();
        if (ch < NUM_CH) { g_mode = Mode::Hold; solo((uint8_t)ch); }
        else { Serial.print(F("channel out of range (0..")); Serial.print(NUM_CH - 1); Serial.println(F(")")); }
        if (c == '\r' || c == '\n') return;
    }

    switch (c) {
        case 'n': g_mode = Mode::Hold; solo((g_solo + 1) % NUM_CH); break;
        case 'p': g_mode = Mode::Hold; solo((g_solo + NUM_CH - 1) % NUM_CH); break;
        case 'w': g_mode = Mode::Walk;  g_pos = 0; g_lastStep = 0; Serial.println(F("walk")); break;
        case 'c': g_mode = Mode::Chase; g_pos = 0; g_lastStep = 0; Serial.println(F("chase")); break;
        case 'a': g_mode = Mode::Hold; setAll(allMask()); Serial.println(F("all on")); break;
        case 'o': g_mode = Mode::Hold; setAll(0); Serial.println(F("all off")); break;
        case '.': g_mode = Mode::Hold; Serial.println(F("hold")); break;
        case 'e':
            g_expOn = !g_expOn;
            g_expLast[0] = g_expLast[1] = -100;  // reprint both on enable
            Serial.println(g_expOn ? F("expression readout on") : F("expression readout off"));
            break;
        case '?': printMenu(); break;
        default: break;
    }
}

static void stepAnimation() {
    if (g_mode == Mode::Hold) return;
    uint32_t now = millis();
    uint16_t period = g_mode == Mode::Walk ? g_stepMs : 80;
    if (now - g_lastStep < period) return;
    g_lastStep = now;

    if (g_mode == Mode::Walk) solo(g_pos);
    else                      setAll(1ULL << g_pos);
    g_pos = (g_pos + 1) % NUM_CH;
}

static void pollExp() {
    if (!g_expOn || g_numAcc >= 0) return;  // don't interleave with typing
    uint32_t now = millis();
    if (now - g_expLastMs < EXP_PERIOD) return;
    g_expLastMs = now;

    for (uint8_t i = 0; i < 2; ++i) {
        int v = analogRead(PIN_EXP[i]);
        if (abs(v - g_expLast[i]) >= EXP_HYST) {
            g_expLast[i] = v;
            printExp(i, v);
        }
    }
}

void loop() {
    while (Serial.available()) handleChar((char)Serial.read());
    stepAnimation();
    pollExp();
    pollSwitches();
}
