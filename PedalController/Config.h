#pragma once

#include <stdint.h>

namespace Config {

constexpr uint8_t PIN_MIDI_TX   = 1;

constexpr uint8_t PIN_MUX_SEL_A = 2;
constexpr uint8_t PIN_MUX_SEL_B = 3;
constexpr uint8_t PIN_MUX_SEL_C = 4;

constexpr uint8_t PIN_MUX_Z[3]  = {5, 6, 8};

constexpr uint8_t PIN_LED_LATCH = 10;
constexpr uint8_t PIN_LED_DATA  = 11;
constexpr uint8_t PIN_LED_CLK   = 13;

// LEDs are common anode: anodes tied to V+, 595 output LOW = LED on.
constexpr bool    LED_ACTIVE_LOW = true;

constexpr uint8_t PIN_EXP1      = 14;
constexpr uint8_t PIN_EXP2      = 15;

constexpr uint8_t NUM_MUXES     = 3;
constexpr uint8_t CHANS_PER_MUX = 8;
constexpr uint8_t NUM_SWITCHES  = NUM_MUXES * CHANS_PER_MUX;

constexpr uint32_t SCAN_PERIOD_US = 1000;
constexpr uint8_t  MUX_SETTLE_US  = 5;
constexpr uint8_t  DEBOUNCE_N     = 4;

}
