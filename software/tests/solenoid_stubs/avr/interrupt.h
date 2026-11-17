#pragma once

#include <stdint.h>

#define _BV(bit) (1U << (bit))
#define ISR(vector) void vector()

enum { CS10 = 0, CS11 = 1, CS12 = 2, WGM12 = 3, OCIE1A = 1, OCF1A = 1, PD6 = 6 };
inline uint8_t TCCR1A = 0, TCCR1B = 0, TIMSK1 = 0, TIFR1 = 0, PORTD = 0;
inline uint16_t TCNT1 = 0, OCR1A = 0;
