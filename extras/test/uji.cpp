// Uji logika TimbanganHX711 di PC dengan HX711 tiruan di level pin (Arduino.h):
//   g++ -std=c++11 -Wall -Wextra -I. -I../../src uji.cpp ../../src/TimbanganHX711.cpp -o uji && ./uji
#include <assert.h>
#include <stdio.h>
#include "TimbanganHX711.h"

uint64_t waktuUs = 0;
ChipHX711 *daftarChip[4] = {};
bool interruptAktif = true, isrPanjang = false;
uint32_t isrUs = 120, interruptMatiTerlama = 0;
uint64_t interruptMatiSejak = 0;

// Load cell tiruan: nol di 84.000, 420 hitungan per gram di gain 128, noise ±20.
static float beratAsli = 0;
static long loadCell(uint8_t gain) {
  long v = lroundf((84000 + beratAsli * 420) * gain / 128.0f);
  return v + rand() % 41 - 20;
}
// Kanal B (gain 32) dipakai untuk sinyal lain: nilai tetap yang mudah dikenali.
static long denganKanalB(uint8_t gain) { return gain == 32 ? 123456 : loadCell(gain); }

static bool dekat(float a, float b, float tol) { return fabsf(a - b) <= tol; }

// Jalankan loop() selama ms, perbarui() setiap 1 ms. Kembalikan waktu terlama satu perbarui() (µs).
static uint32_t jalan(TimbanganHX711 &t, uint32_t ms) {
  uint32_t terlama = 0;
  uint64_t akhir = waktuUs + (uint64_t)ms * 1000;
  while (waktuUs < akhir) {
    uint64_t t0 = waktuUs;
    t.perbarui();
    uint32_t d = (uint32_t)(waktuUs - t0);
    if (d > terlama) terlama = d;
    if (waktuUs < t0 + 1000) waktuUs = t0 + 1000;
  }
  return terlama;
}

static ChipHX711 chip, chip2;

static void siapkan(TimbanganHX711 &t, uint64_t mulai = 0) {
  chip = ChipHX711();
  chip.nilai = loadCell;
  chip.konversiBerikut = mulai + chip.periodeUs;
  daftarChip[0] = &chip;
  daftarChip[1] = nullptr;
  waktuUs = mulai;
  beratAsli = 0;
  t = TimbanganHX711(chip.pinDt, chip.pinSck);
  t.mulai();
}

int main() {
  int kasus = 0;
  { // settling: 5 bacaan pertama dibuang, lalu OK; berat NAN sebelum siap
    TimbanganHX711 t;
    siapkan(t);
    assert(t.status() == TimbanganHX711::BELUM_SIAP && isnan(t.berat()));
    jalan(t, 450);
    assert(t.status() == TimbanganHX711::BELUM_SIAP);
    jalan(t, 200);
    assert(t.siap() && dekat(t.mentah(), 84000, 25));
    kasus++;
  }
  { // tara + kalibrasi 500 g, lalu timbang 250 g dan 1 kg; offset/faktor bisa dipindah ke objek lain
    TimbanganHX711 t;
    siapkan(t);
    jalan(t, 700);
    t.tara();
    assert(t.sedangMengukur());
    jalan(t, 1100);
    assert(!t.sedangMengukur() && dekat(t.offset(), 84000, 10));
    beratAsli = 500;
    jalan(t, 500);
    t.kalibrasi(500);
    assert(isnan(t.berat())); // selama kalibrasi faktor belum ada
    jalan(t, 1100);
    assert(!t.sedangMengukur() && dekat(t.faktor(), 420, 0.5f));
    beratAsli = 250;
    jalan(t, 2000); // penghalusan bawaan (~4 sampel) butuh ±1,5 s di 10 SPS
    assert(dekat(t.berat(), 250, 0.5f));
    beratAsli = 1000;
    jalan(t, 2000);
    assert(dekat(t.berat(), 1000, 0.5f));
    TimbanganHX711 u; // kalibrasi tersimpan (mis. EEPROM) dipakai lagi tanpa ulang
    long o = t.offset();
    float f = t.faktor();
    siapkan(u, waktuUs);
    beratAsli = 1000;
    u.aturOffset(o);
    u.aturFaktor(f);
    jalan(u, 1000);
    assert(dekat(u.berat(), 1000, 0.5f));
    kasus++;
  }
  { // sensor tidak terhubung sejak awal: tidak pernah menunggu, status setelah 0,5 s
    TimbanganHX711 t;
    siapkan(t);
    chip.keadaan = ChipHX711::LEPAS;
    uint32_t terlama = jalan(t, 400);
    assert(t.status() == TimbanganHX711::BELUM_SIAP);
    uint32_t d = jalan(t, 200);
    if (d > terlama) terlama = d;
    assert(t.status() == TimbanganHX711::TIDAK_TERHUBUNG && isnan(t.berat()));
    assert(terlama < 20); // satu digitalRead, bukan menunggu
    kasus++;
  }
  { // kabel dicabut lalu dipasang lagi: pulih sendiri tanpa reset program
    TimbanganHX711 t;
    siapkan(t);
    beratAsli = 0;
    jalan(t, 700);
    t.tara();
    jalan(t, 1100);
    chip.keadaan = ChipHX711::LEPAS;
    jalan(t, 600);
    assert(t.status() == TimbanganHX711::TIDAK_TERHUBUNG);
    chip.keadaan = ChipHX711::NORMAL;
    chip.settle = 4; // modul baru tersambung = baru menyala
    jalan(t, 300);
    assert(t.status() == TimbanganHX711::BELUM_SIAP); // bacaan settling tidak dipakai
    jalan(t, 500);
    assert(t.siap() && dekat(t.berat(), 0, 25)); // offset lama masih berlaku (faktor 1 = hitungan mentah)
    kasus++;
  }
  { // VCC modul lepas: DT tertarik LOW, yang terbaca bukan HX711 (pulsa ke-25 tidak menarik DT HIGH)
    TimbanganHX711 t;
    siapkan(t);
    jalan(t, 700);
    chip.keadaan = ChipHX711::VCC_LEPAS;
    jalan(t, 5);
    assert(t.status() == TimbanganHX711::TIDAK_TERHUBUNG && isnan(t.berat()));
    kasus++;
  }
  { // ADC jenuh (beban terlalu berat / kabel load cell salah)
    TimbanganHX711 t;
    siapkan(t);
    jalan(t, 700);
    beratAsli = 30000; // 84000 + 30000*420 > 0x7FFFFF
    jalan(t, 300);
    assert(t.status() == TimbanganHX711::DI_LUAR_RENTANG && isnan(t.berat()));
    beratAsli = 0;
    jalan(t, 300);
    assert(t.siap());
    kasus++;
  }
  { // ISR panjang (120 µs) di tengah pembacaan: interrupt hanya mati selama satu pulsa,
    // SCK tidak pernah tinggi > 60 µs, data tidak rusak
    TimbanganHX711 t;
    siapkan(t);
    isrPanjang = true;
    interruptMatiTerlama = 0;
    jalan(t, 700);
    t.tara();
    jalan(t, 1100);
    beratAsli = 100;
    jalan(t, 2000);
    isrPanjang = false;
    assert(chip.jumlahPowerDown == 0 && chip.jumlahBacaRusak == 0);
    assert(interruptMatiTerlama <= 2 * BIAYA_PIN_US + 1); // digitalWrite HIGH + 1 µs (non-AVR) + LOW
    assert(dekat((t.mentah() - t.offset()) / 420.0f, 100, 0.2f));
    printf("interrupt mati terlama: %u us (tiruan, %u us per akses pin)\n", (unsigned)interruptMatiTerlama,
           (unsigned)BIAYA_PIN_US);
    kasus++;
  }
  { // gain 64 (27 pulsa) dan kanal B (26 pulsa) dipilih untuk konversi berikutnya, plus settling
    TimbanganHX711 t;
    siapkan(t);
    chip.nilai = denganKanalB;
    jalan(t, 700);
    t.aturGain(64);
    assert(t.status() == TimbanganHX711::BELUM_SIAP);
    jalan(t, 700);
    assert(t.siap() && chip.gain == 64 && dekat(t.mentah(), 84000 / 2, 25));
    t.aturGain(32);
    jalan(t, 700);
    assert(t.siap() && chip.gain == 32 && t.mentah() == 123456);
    kasus++;
  }
  { // tidur: SCK tetap HIGH (> 60 µs) = power down; bangun: reset, settling, lalu normal
    TimbanganHX711 t;
    siapkan(t);
    jalan(t, 700);
    t.tidur();
    jalan(t, 1000);
    chip.jalan(); // tiruan menghitung keadaan saat diperiksa
    assert(t.status() == TimbanganHX711::TIDUR && chip.mati);
    t.bangun();
    assert(!chip.mati && t.status() == TimbanganHX711::BELUM_SIAP);
    jalan(t, 700);
    assert(t.siap() && dekat(t.mentah(), 84000, 25));
    kasus++;
  }
  { // millis() meluap (dan 16 bit internal berputar berkali-kali): status tetap benar
    TimbanganHX711 t;
    siapkan(t, (uint64_t)0xFFFFFF00u * 1000 - 300000);
    jalan(t, 1000);
    assert(t.siap());
    jalan(t, 70000); // > 65,5 s: penghitung 16 bit berputar
    assert(t.siap());
    chip.keadaan = ChipHX711::LEPAS;
    jalan(t, 600);
    assert(t.status() == TimbanganHX711::TIDAK_TERHUBUNG);
    kasus++;
  }
  { // dua timbangan sekaligus, pin berbeda
    TimbanganHX711 a, b;
    siapkan(a);
    chip2 = ChipHX711();
    chip2.pinDt = 2;
    chip2.pinSck = 3;
    chip2.nilai = [](uint8_t) -> long { return -5000; };
    daftarChip[1] = &chip2;
    b = TimbanganHX711(2, 3);
    b.mulai();
    for (int i = 0; i < 800; i++) {
      a.perbarui();
      b.perbarui();
      waktuUs += 1000;
    }
    assert(a.siap() && b.siap() && dekat(a.mentah(), 84000, 25) && b.mentah() == -5000);
    kasus++;
  }
  { // tanpa pin: bacaan dari ADC lain lewat perbarui(long); penghalusan; masukan tidak sah
    TimbanganHX711 t;
    t.mulai();
    for (int i = 0; i < 5; i++) assert(!t.perbarui(1000L)); // settling
    assert(t.perbarui(1000L) && t.siap());
    t.aturPenghalusan(0);
    t.perbarui(2000L);
    assert(t.berat() == 2000); // faktor 1, offset 0, tanpa penghalusan
    t.aturPenghalusan(1);      // alfa 1/2
    t.perbarui(3000L);
    assert(t.berat() == 2500);
    t.kalibrasi(0);   // ditolak
    t.kalibrasi(NAN); // ditolak
    assert(!t.sedangMengukur() && t.faktor() == 1);
    t.tara(0); // minimal 1 sampel
    t.perbarui(3000L);
    assert(!t.sedangMengukur() && t.offset() == 3000);
    t.perbarui(-0x800000L);
    assert(t.status() == TimbanganHX711::DI_LUAR_RENTANG);
    assert(!TimbanganHX711().perbarui()); // tanpa pin, perbarui() tidak membaca apa pun
    kasus++;
  }
  printf("Semua uji lolos (%d kasus, sizeof = %u byte)\n", kasus, (unsigned)sizeof(TimbanganHX711));
  return 0;
}
