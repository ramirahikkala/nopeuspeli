#pragma once
#include <stdint.h>

// Lamppu- ja nappipinnit. Numerointi on fyysinen (lamppu/nappi n kotelossa).
// Mitattu iomap-ohjelmalla, ks. docs/kytkenta.md.
//
// Lamppu n ja nappi n ovat pari: pelissä lampun n sytyttyä painetaan nappia n.
// LEDit: active HIGH (ULN2803 low-side). Napit: active LOW, sisäinen pull-up.

static const uint8_t NUM_PADS = 4;
static const uint8_t LED_PINS[NUM_PADS] = {2, 10, 3, 1};
static const uint8_t BTN_PINS[NUM_PADS] = {21, 18, 17, 43};
