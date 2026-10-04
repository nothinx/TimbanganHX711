// TimbanganHX711 - Copyright (c) 2026 Amadeo Wisesa. Lisensi MIT.
// Nomor halaman merujuk ke datasheet HX711 (Avia Semiconductor), lihat extras/riset.md.
#include "TimbanganHX711.h"
#include <math.h>

// Hlm. 3: settling 400 ms di 10 SPS dan 50 ms di 80 SPS, keduanya 4 periode konversi.
// Jadi cukup buang bacaan pertama, berapa pun RATE di modul: 4 periode settling + 1, karena
// gain baru baru dikirim lewat pulsa clock bacaan pertama (hlm. 4) dan settling mulai dari situ.
static const uint8_t SAMPEL_SETTLING = 5;
// Periode konversi paling lama 100 ms (10 SPS, hlm. 3). Tanpa data 5x lebih lama = tidak terhubung.
// Waktu disimpan dalam satuan 8 ms (8 bit, berputar tiap 2 detik).
// ponytail: jika perbarui() dipanggil lebih jarang dari tiap 2 detik saat kabel lepas, deteksi bisa
// terlambat; pakai 16 bit bila ada pemakaian seperti itu.
static const uint8_t BATAS_DIAM = 500 / 8;
static uint8_t waktu8() { return (uint8_t)((uint16_t)millis() >> 3); } // geser 16 bit lebih murah
// Hlm. 4: data di luar rentang jenuh di 800000h (MIN) atau 7FFFFFh (MAX).
static const long JENUH_MAX = 0x7FFFFFL, JENUH_MIN = -0x800000L;

void TimbanganHX711::mulai() {
  if (_dt != TANPA_PIN) {
    // Pull-up: DT yang lepas terbaca HIGH = "belum siap", bukan data palsu.
    pinMode(_dt, INPUT_PULLUP);
    pinMode(_sck, OUTPUT);
    digitalWrite(_sck, LOW); // hlm. 5: SCK LOW = kerja normal (juga bangun dari power down)
  }
  mulaiSettling();
}

void TimbanganHX711::mulaiSettling() {
  _settle = SAMPEL_SETTLING;
  _status = BELUM_SIAP;
  _adaHalus = 0;
  _waktu = waktu8();
}

// Hlm. 4-5: setiap pulsa SCK menggeser satu bit (MSB dulu), 24 bit data lalu
// 1-3 pulsa untuk memilih kanal/gain konversi berikutnya.
// Hlm. 5: SCK tinggi maksimal 50 µs (T3); lebih dari 60 µs = power down dan data rusak.
// Karena itu interrupt dimatikan hanya selama pulsa tinggi, bukan sepanjang 25-27 bit.
bool TimbanganHX711::baca24(long &hasil) {
  uint8_t pulsa = 25 + _pulsa;
  long x = 0;
#ifdef __AVR__
  // Register port langsung: digitalWrite/digitalRead ±70 siklus per panggilan.
  volatile uint8_t *keluar = portOutputRegister(digitalPinToPort(_sck));
  volatile uint8_t *masuk = portInputRegister(digitalPinToPort(_dt));
  uint8_t bitSck = digitalPinToBitMask(_sck), bitDt = digitalPinToBitMask(_dt);
  for (uint8_t i = 0; i < pulsa; i++) {
    uint8_t sreg = SREG;
    cli();
    *keluar |= bitSck;
    __asm__ __volatile__("nop\n\tnop\n\tnop\n\t"); // T3 minimal 0,2 µs (3,2 siklus di 16 MHz)
    *keluar &= ~bitSck;
    SREG = sreg; // pulihkan, bukan sekadar sei(): aman dipanggil saat interrupt sudah mati
    __asm__ __volatile__("nop\n\tnop\n\t"); // T2: data valid 0,1 µs setelah tepi naik
    if (i < 24) x = (x << 1) | ((*masuk & bitDt) != 0);
  }
  bool selesai = (*masuk & bitDt) != 0;
#else
  for (uint8_t i = 0; i < pulsa; i++) {
    noInterrupts();
    digitalWrite(_sck, HIGH);
    delayMicroseconds(1); // T3 minimal 0,2 µs; MCU cepat (ESP32, STM32) bisa lebih singkat
    digitalWrite(_sck, LOW);
    interrupts();
    if (i < 24) x = (x << 1) | (digitalRead(_dt) == HIGH); // bit valid sampai pulsa berikutnya
    delayMicroseconds(1); // T4 minimal 0,2 µs
  }
  bool selesai = digitalRead(_dt) == HIGH;
#endif
  if (x & 0x800000L) x |= ~0xFFFFFFL; // komplemen dua 24 bit -> long
  hasil = x;
  // Hlm. 4: pulsa ke-25 menarik DT kembali HIGH. Jika tetap LOW, yang terbaca bukan HX711
  // (mis. VCC modul lepas sehingga DT tertarik ke GND).
  return selesai;
}

// DT HIGH? Di AVR langsung dari register port (digitalRead ±50 siklus), dipanggil tiap loop().
static bool dtTinggi(uint8_t pin) {
#ifdef __AVR__
  return *portInputRegister(digitalPinToPort(pin)) & digitalPinToBitMask(pin);
#else
  return digitalRead(pin) == HIGH;
#endif
}

bool TimbanganHX711::perbarui() {
  if (_dt == TANPA_PIN || _status == TIDUR) return false;
  if (dtTinggi(_dt)) { // hlm. 4: DT HIGH = data belum siap
    if (_status != TIDAK_TERHUBUNG && (uint8_t)(waktu8() - _waktu) >= BATAS_DIAM) {
      _status = TIDAK_TERHUBUNG;
      _settle = SAMPEL_SETTLING; // saat tersambung lagi, HX711 mulai dari reset
      _adaHalus = 0;
    }
    return false;
  }
  long x;
  bool sah = baca24(x);
  _waktu = waktu8();
  if (!sah) {
    _status = TIDAK_TERHUBUNG;
    _settle = SAMPEL_SETTLING;
    _adaHalus = 0;
    return false;
  }
  return perbarui(x);
}

bool TimbanganHX711::perbarui(long x) {
  if (x >= JENUH_MAX || x <= JENUH_MIN) {
    _status = DI_LUAR_RENTANG;
    return false;
  }
  if (_settle) {
    _settle--;
    _status = BELUM_SIAP;
    return false;
  }
  _status = OK;
  if (_target) { // tara/kalibrasi: rata-rata berjalan di _halus, tanpa akumulator terpisah
    _halus += (x - _halus) / ++_hitung;
    if (_hitung < _target) return false;
    if (_kalibrasi) _faktor = (_halus - _offset) / _faktor; // _faktor berisi beban yang diketahui
    else _offset = lroundf(_halus);
    _target = 0;
    _adaHalus = 1; // rata-rata menjadi titik awal penghalusan
    return true;
  }
  // Penghalusan EMA dengan alfa = 1/2^n (ldexpf: pangkat dua tanpa pembagian).
  if (!_adaHalus || !_halusN) _halus = x;
  else _halus += ldexpf(x - _halus, -(int)_halusN);
  _adaHalus = 1;
  return true;
}

float TimbanganHX711::berat() const {
  if (_status != OK || _target || _faktor == 0) return NAN;
  return (_halus - _offset) / _faktor;
}

void TimbanganHX711::tara(uint8_t sampel) {
  _kalibrasi = 0;
  _hitung = 0;
  _halus = 0;
  _target = sampel ? sampel : 1;
}

void TimbanganHX711::kalibrasi(float bebanDiketahui, uint8_t sampel) {
  if (bebanDiketahui == 0 || !isfinite(bebanDiketahui)) return; // 0, NAN, tak hingga ditolak
  _kalibrasi = 1;
  _faktor = bebanDiketahui;
  _hitung = 0;
  _halus = 0;
  _target = sampel ? sampel : 1;
}

void TimbanganHX711::aturGain(uint8_t gain) {
  _pulsa = gain == 64 ? 2 : gain == 32 ? 1 : 0; // hlm. 4, tabel 3: 27, 26, 25 pulsa
  mulaiSettling(); // hlm. 3: ganti gain/kanal butuh settling
}

void TimbanganHX711::tidur() {
  if (_dt != TANPA_PIN) digitalWrite(_sck, HIGH); // hlm. 5: SCK HIGH > 60 µs = power down
  _status = TIDUR;
}

void TimbanganHX711::bangun() {
  if (_dt != TANPA_PIN) digitalWrite(_sck, LOW); // hlm. 5: reset, kembali ke kanal A gain 128
  mulaiSettling(); // konversi pertama setelah reset selalu A/128, ikut dibuang
}

const __FlashStringHelper *TimbanganHX711::alasan() const {
  switch (_status) {
    case OK: return F("OK");
    case BELUM_SIAP: return F("Menunggu HX711 stabil (settling), tunggu sebentar");
    case TIDAK_TERHUBUNG:
      return F("HX711 tidak mengirim data: cek kabel VCC, GND, DT, SCK dan nomor pin di program");
    case DI_LUAR_RENTANG:
      return F("Bacaan jenuh: beban melebihi kapasitas, atau kabel load cell (E+, E-, A+, A-) salah");
    case TIDUR: return F("HX711 sedang tidur, panggil bangun()");
  }
  return F("");
}
