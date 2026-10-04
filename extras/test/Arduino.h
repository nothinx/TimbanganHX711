// Arduino.h tiruan + HX711 tiruan di level pin, mengikuti datasheet Avia Semiconductor:
// - DT HIGH saat data belum siap, LOW saat siap (hlm. 4).
// - Tiap pulsa SCK menggeser 1 bit MSB dulu; 25/26/27 pulsa memilih A-128/B-32/A-64 untuk
//   konversi berikutnya; pulsa ke-25 menarik DT HIGH (hlm. 4, tabel 3).
// - SCK HIGH > 60 µs = power down; SCK kembali LOW = reset ke A-128 (hlm. 5).
// - 4 konversi pertama setelah reset/ganti gain belum stabil (settling, hlm. 3).
// - Data jenuh di 0x800000 / 0x7FFFFF (hlm. 4).
// Waktu disimulasikan dalam µs: setiap akses pin memakan BIAYA_PIN_US, dan "interrupt"
// tiruan (ISR panjang) hanya bisa terjadi saat interrupt aktif.
#pragma once
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2

class __FlashStringHelper;
#define F(s) (reinterpret_cast<const __FlashStringHelper *>(s))

extern uint64_t waktuUs;
inline uint32_t millis() { return (uint32_t)(waktuUs / 1000); }
inline uint32_t micros() { return (uint32_t)waktuUs; }
inline void delayMicroseconds(unsigned us) { waktuUs += us; }

struct ChipHX711 {
  enum Keadaan { NORMAL, LEPAS, VCC_LEPAS };
  uint8_t pinDt = 0, pinSck = 1;
  Keadaan keadaan = NORMAL;
  uint32_t periodeUs = 100000;          // 10 SPS
  long (*nilai)(uint8_t gain) = nullptr; // bacaan ideal untuk gain ini
  bool sck = false, siap = false, mati = false;
  uint64_t sckNaik = 0, konversiBerikut = 100000;
  long data = 0;
  int pulsa = 0, settle = 4;
  uint8_t gain = 128;
  int jumlahPowerDown = 0, jumlahBacaRusak = 0;

  void jalan() { // perbarui konversi sampai waktu sekarang
    if (sck && waktuUs - sckNaik > 60 && !mati) { mati = true; jumlahPowerDown++; siap = false; }
    if (mati) return;
    while (waktuUs >= konversiBerikut) {
      konversiBerikut += periodeUs;
      if (pulsa > 0 && pulsa < 25) jumlahBacaRusak++; // pembacaan sebelumnya terpotong
      if (pulsa >= 25) { // jumlah pulsa bacaan terakhir memilih gain konversi ini (tabel 3)
        uint8_t g = pulsa == 25 ? 128 : pulsa == 26 ? 32 : 64;
        if (g != gain) { gain = g; settle = 4; }
      }
      pulsa = 0;
      long v = nilai ? nilai(gain) : 0;
      if (settle > 0) { settle--; v += 50000; } // belum stabil: meleset jauh
      if (v > 0x7FFFFF) v = 0x7FFFFF;
      if (v < -0x800000) v = -0x800000;
      data = v & 0xFFFFFF;
      siap = true;
    }
  }
  void tulisSck(bool v) {
    jalan();
    if (v && !sck) {
      sckNaik = waktuUs;
      if (siap || pulsa >= 24) {
        pulsa++;
        if (pulsa >= 25) siap = false;
      }
    }
    if (!v && sck && mati) { // reset setelah power down
      mati = false; gain = 128; settle = 4; pulsa = 0; siap = false;
      konversiBerikut = waktuUs + periodeUs;
    }
    sck = v;
  }
  int bacaDt() {
    jalan();
    if (keadaan == LEPAS) return HIGH;     // pull-up MCU
    if (keadaan == VCC_LEPAS) return LOW;  // tertarik ke GND
    if (mati) return HIGH;
    if (pulsa >= 1 && pulsa <= 24) return (data >> (24 - pulsa)) & 1;
    return siap ? LOW : HIGH;
  }
};

extern ChipHX711 *daftarChip[4];
extern bool interruptAktif, isrPanjang;
extern uint32_t isrUs, interruptMatiTerlama;
extern uint64_t interruptMatiSejak;
static const uint32_t BIAYA_PIN_US = 2;

inline ChipHX711 *chipDi(uint8_t pin, bool sck) {
  for (ChipHX711 *c : daftarChip)
    if (c && (sck ? c->pinSck : c->pinDt) == pin) return c;
  return nullptr;
}
inline void mungkinIsr() { // ISR panjang (mis. Serial/servo) bisa menyela di mana saja saat interrupt aktif
  if (interruptAktif && isrPanjang && rand() % 4 == 0) waktuUs += isrUs;
}
inline void pinMode(uint8_t, uint8_t) {}
inline void digitalWrite(uint8_t pin, uint8_t v) {
  waktuUs += BIAYA_PIN_US;
  mungkinIsr();
  if (ChipHX711 *c = chipDi(pin, true)) c->tulisSck(v);
}
inline int digitalRead(uint8_t pin) {
  waktuUs += BIAYA_PIN_US;
  mungkinIsr();
  ChipHX711 *c = chipDi(pin, false);
  return c ? c->bacaDt() : HIGH;
}
inline void noInterrupts() { interruptAktif = false; interruptMatiSejak = waktuUs; }
inline void interrupts() {
  uint32_t d = (uint32_t)(waktuUs - interruptMatiSejak);
  if (!interruptAktif && d > interruptMatiTerlama) interruptMatiTerlama = d;
  interruptAktif = true;
}
