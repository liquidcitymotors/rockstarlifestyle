#include "Leds.h"
#include "Config.h"

#include <Arduino.h>
#include <SPI.h>

namespace Leds {

namespace {

constexpr uint8_t  NUM_CHIPS = 4;  // 32 channels: exactly fills g_state
constexpr uint32_t SPI_HZ    = 1000000;  // slow on purpose: long jumper wires to the 595s

uint32_t g_state    = 0;
uint32_t g_shadowed = 0;
bool     g_flushed  = false;

}

void begin() {
    pinMode(Config::PIN_LED_LATCH, OUTPUT);
    digitalWriteFast(Config::PIN_LED_LATCH, LOW);

    SPI.begin();
    update();
}

void set(uint8_t idx, bool on) {
    if (idx >= NUM_CHIPS * 8) return;
    if (on) g_state |=  (1UL << idx);
    else    g_state &= ~(1UL << idx);
}

void setAll(uint32_t mask) { g_state = mask; }

uint32_t state() { return g_state; }

void update() {
    if (g_flushed && g_state == g_shadowed) return;
    g_shadowed = g_state;
    g_flushed  = true;

    const uint32_t out = Config::LED_ACTIVE_LOW ? ~g_state : g_state;

    SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    for (int8_t chip = NUM_CHIPS - 1; chip >= 0; --chip)
        SPI.transfer((out >> (chip * 8)) & 0xFF);
    SPI.endTransaction();

    digitalWriteFast(Config::PIN_LED_LATCH, HIGH);
    delayMicroseconds(1);  // 595 needs a latch pulse wider than ~50 ns at 3.3V
    digitalWriteFast(Config::PIN_LED_LATCH, LOW);
}

}
