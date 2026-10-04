// Pengukur siklus CPU ATmega328P untuk simavr: Timer1 tanpa prescaler.
// UKUR(nama, ulang, kode): cetak "BENCH nama siklus_per_panggilan".
#pragma once
#include <Arduino.h>
#include <avr/sleep.h>
static volatile uint16_t _ovf;
ISR(TIMER1_OVF_vect) { _ovf++; }
static inline void siklusMulai() { TCCR1A = 0; TCCR1B = 0; TCNT1 = 0; _ovf = 0; TIFR1 = 0xFF; TIMSK1 = _BV(TOIE1); TCCR1B = _BV(CS10); }
static inline uint32_t siklusAkhir() { uint16_t t = TCNT1; TCCR1B = 0; uint32_t c = ((uint32_t)_ovf << 16) | t; if ((TIFR1 & _BV(TOV1)) && t < 0x8000) c += 65536UL; return c; }
#define UKUR(nama, ulang, kode) do { \
    noInterrupts(); uint8_t t0 = TIMSK0; TIMSK0 = 0; interrupts(); \
    siklusMulai(); for (uint16_t _i = 0; _i < (ulang); _i++) { kode; } uint32_t _c = siklusAkhir(); \
    TIMSK0 = t0; Serial.print(F("BENCH ")); Serial.print(F(nama)); Serial.print(' '); \
    Serial.println((float)_c / (ulang), 1); Serial.flush(); } while (0)
static inline void selesai() { Serial.println(F("BENCH_SELESAI")); Serial.flush(); cli(); set_sleep_mode(SLEEP_MODE_PWR_DOWN); sleep_enable(); sleep_cpu(); }
