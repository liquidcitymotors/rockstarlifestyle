#include "Leds.h"
#include "Config.h"

#include <Arduino.h>
#include <SPI.h>

namespace Leds {

namespace {

constexpr uint8_t  NUM_CHIPS = 3;
constexpr uint32_t SPI_HZ    = 8000000;

uint32_t g_state    = 0;
uint32_t g_shadowed = 0xFFFFFFFFUL;

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
    if (g_state == g_shadowed) return;
    g_shadowed = g_state;

    const uint32_t out = Config::LED_ACTIVE_LOW ? ~g_state : g_state;

    SPI.beginTransaction(SPISettings(SPI_HZ, MSBFIRST, SPI_MODE0));
    SPI.transfer((out >> 16) & 0xFF);
    SPI.transfer((out >>  8) & 0xFF);
    SPI.transfer((out      ) & 0xFF);
    SPI.endTransaction();

    digitalWriteFast(Config::PIN_LED_LATCH, HIGH);
    digitalWriteFast(Config::PIN_LED_LATCH, LOW);
}

}
