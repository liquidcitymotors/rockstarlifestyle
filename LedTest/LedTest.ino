// LedTest — standalone bring-up sketch for the 18-LED driver board.
//
// Self-contained on purpose: it re-declares the same pins and the same
// 74HC595 SPI driver as PedalController/Leds.cpp so you can flash it
// without disturbing the main firmware, but it exercises the identical
// shift-out path that will ship.
//
// Hardware: Teensy 3.2 -> 3x M74HC595 daisy chain over 5-pin connector
//   V+ (3.3V to start), GND, DATA=pin11 (MOSI), CLK=pin13 (SCK), LATCH=pin10
//
// Build/flash:
//   arduino-cli compile --fqbn teensy:avr:teensy31:usb=serial LedTest
//   arduino-cli upload  --fqbn teensy:avr:teensy31:usb=serial -p <port> LedTest
//
// Serial menu @ 115200 — type a key + Enter (or just the key):
//   w        walk: light one LED at a time, 0..17, printing each index
//   a        all on
//   o        all off
//   c        chase (single dot sweeping, looping)
//   k        knight-rider style bounce
//   b        binary count on the low bits
//   <number> toggle that LED index (0..17)
//   .        stop any running animation, hold current state
//   ?        reprint this menu

#include <Arduino.h>
#include <SPI.h>

// ---- pins (mirror PedalController/Config.h) ----
constexpr uint8_t PIN_LED_LATCH = 10;
constexpr uint8_t PIN_LED_DATA  = 11;  // SPI0 MOSI
constexpr uint8_t PIN_LED_CLK   = 13;  // SPI0 SCK

constexpr uint8_t  NUM_CHIPS = 3;
constexpr uint8_t  NUM_LEDS  = 18;     // 18 wired of 24 available
constexpr uint32_t SPI_HZ    = 8000000;

// LEDs are common anode: anodes tied to V+, 595 output LOW = LED on.
constexpr bool LED_ACTIVE_LOW = true;

static uint32_t g_state = 0;

// ---- 74HC595 driver (mirrors Leds.cpp) ----
static void ledsFlush() {
    const uint32_t out = LED_ACTIVE_LOW ? ~g_state : g_state;

    SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    SPI.transfer((out >> 16) & 0xFF);
    SPI.transfer((out >>  8) & 0xFF);
    SPI.transfer((out      ) & 0xFF);
    SPI.endTransaction();

    digitalWriteFast(PIN_LED_LATCH, HIGH);
    digitalWriteFast(PIN_LED_LATCH, LOW);
}

static void setAll(uint32_t mask) { g_state = mask; ledsFlush(); }
static void setOne(uint8_t idx, bool on) {
    if (idx >= NUM_CHIPS * 8) return;
    if (on) g_state |=  (1UL << idx);
    else    g_state &= ~(1UL << idx);
    ledsFlush();
}

// ---- animation state ----
enum class Mode { Hold, Walk, Chase, Knight, Binary };
static Mode     g_mode      = Mode::Hold;
static uint32_t g_lastStep  = 0;
static uint16_t g_stepMs    = 250;
static uint8_t  g_pos       = 0;
static int8_t   g_dir       = 1;
static uint32_t g_counter   = 0;

static void printMenu() {
    Serial.println();
    Serial.println(F("=== LED bring-up ==="));
    Serial.println(F("  w  walk one LED at a time (0..17, prints index)"));
    Serial.println(F("  a  all on        o  all off"));
    Serial.println(F("  c  chase         k  knight-rider bounce"));
    Serial.println(F("  b  binary count  .  stop/hold"));
    Serial.println(F("  <number>  toggle that LED (0..17)"));
    Serial.println(F("  ?  this menu"));
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    pinMode(PIN_LED_LATCH, OUTPUT);
    digitalWriteFast(PIN_LED_LATCH, LOW);
    SPI.begin();
    setAll(0);

    uint32_t t0 = millis();
    while (!Serial && (millis() - t0) < 2000) { /* wait briefly for host */ }
    printMenu();
    Serial.println(F("Tip: start with 'w' to confirm each LED lights in index order."));
}

// Accumulate digits so multi-digit indices (10..17) work.
static int   g_numAcc   = -1;

static void handleChar(char c) {
    if (c >= '0' && c <= '9') {
        g_numAcc = (g_numAcc < 0 ? 0 : g_numAcc) * 10 + (c - '0');
        return;
    }
    // any non-digit terminates a pending number -> toggle it
    if (g_numAcc >= 0) {
        uint8_t idx = (uint8_t)g_numAcc;
        g_numAcc = -1;
        if (idx < NUM_LEDS) {
            bool now = !((g_state >> idx) & 1);
            setOne(idx, now);
            Serial.print(F("LED ")); Serial.print(idx);
            Serial.println(now ? F(" ON") : F(" OFF"));
        } else {
            Serial.print(F("index out of range (0..")); Serial.print(NUM_LEDS - 1); Serial.println(F(")"));
        }
        if (c == '\r' || c == '\n') return; // the terminator was just whitespace
    }

    switch (c) {
        case 'w': g_mode = Mode::Walk;   g_pos = 0; g_lastStep = 0; setAll(0); Serial.println(F("walk")); break;
        case 'c': g_mode = Mode::Chase;  g_pos = 0; g_lastStep = 0; setAll(0); Serial.println(F("chase")); break;
        case 'k': g_mode = Mode::Knight; g_pos = 0; g_dir = 1; g_lastStep = 0; setAll(0); Serial.println(F("knight")); break;
        case 'b': g_mode = Mode::Binary; g_counter = 0; g_lastStep = 0; Serial.println(F("binary")); break;
        case 'a': g_mode = Mode::Hold; setAll((1UL << NUM_LEDS) - 1); Serial.println(F("all on")); break;
        case 'o': g_mode = Mode::Hold; setAll(0); Serial.println(F("all off")); break;
        case '.': g_mode = Mode::Hold; Serial.println(F("hold")); break;
        case '?': printMenu(); break;
        case '\r': case '\n': case ' ': break;
        default: break;
    }
}

static void stepAnimation() {
    uint32_t now = millis();
    if (now - g_lastStep < g_stepMs) return;
    g_lastStep = now;

    switch (g_mode) {
        case Mode::Walk:
            setAll(1UL << g_pos);
            Serial.print(F("LED ")); Serial.println(g_pos);
            g_pos = (g_pos + 1) % NUM_LEDS;
            break;
        case Mode::Chase:
            setAll(1UL << g_pos);
            g_pos = (g_pos + 1) % NUM_LEDS;
            break;
        case Mode::Knight:
            setAll(1UL << g_pos);
            if (g_pos == NUM_LEDS - 1) g_dir = -1;
            else if (g_pos == 0)       g_dir = 1;
            g_pos += g_dir;
            break;
        case Mode::Binary:
            setAll(g_counter & ((1UL << NUM_LEDS) - 1));
            g_counter++;
            break;
        case Mode::Hold:
        default:
            break;
    }
}

void loop() {
    while (Serial.available()) handleChar((char)Serial.read());
    stepAnimation();
}
