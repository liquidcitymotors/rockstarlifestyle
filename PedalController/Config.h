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

// LED channel (bit index for Leds::set) at each physical position, mapped
// by hand with LedTest. NO_LED = nothing found for that position yet.
constexpr uint8_t NO_LED     = 0xFF;
constexpr uint8_t GRID_ROWS  = 3;
constexpr uint8_t GRID_COLS  = 7;
constexpr uint8_t GRID_LED[GRID_ROWS][GRID_COLS] = {
    { 1, 22,  0, 23, 15, 24, NO_LED},
    { 4, 19,  3, 20,  2, 21, NO_LED},
    { 7, 16,  6, 17,  5, 18, NO_LED},
};

// Two-digit display, segments in order a b c d e f g dp
// (a top, b upper right, c lower right, d bottom, e lower left, f upper left, g middle).
constexpr uint8_t DIGIT_SEG[2][8] = {
    {29, NO_LED, 30,  8, 31, NO_LED, NO_LED, NO_LED},  // left digit
    {NO_LED, 13, 26, 12, 27, 11, 28, 14},              // right digit
};
// Channels 9, 10 and 25 light nothing.

constexpr uint8_t PIN_EXP1      = 14;
constexpr uint8_t PIN_EXP2      = 15;

constexpr uint8_t NUM_MUXES     = 3;
constexpr uint8_t CHANS_PER_MUX = 8;
constexpr uint8_t NUM_SWITCHES  = NUM_MUXES * CHANS_PER_MUX;

constexpr uint32_t SCAN_PERIOD_US = 1000;
constexpr uint8_t  MUX_SETTLE_US  = 5;
constexpr uint8_t  DEBOUNCE_N     = 4;

}
