// TimbanganHX711 - timbangan dengan HX711 + load cell, non-blocking.
// Copyright (c) 2026 Amadeo Wisesa. Lisensi MIT.
//
// - perbarui() tidak pernah menunggu: data dibaca hanya jika HX711 sudah siap.
// - Interrupt hanya dimatikan selama satu pulsa clock, bukan selama 25-27 bit,
//   jadi Serial, encoder, dan servo tetap berjalan.
// - Kabel lepas, data jenuh, dan masa settling dilaporkan lewat status()/alasan().
// - Tara dan kalibrasi berjalan di latar belakang; hasilnya (offset, faktor)
//   bisa dibaca dan diisi ulang, misalnya dari EEPROM.
// Datasheet: Avia Semiconductor HX711 (lihat extras/riset.md).
#pragma once
#include <Arduino.h>

class TimbanganHX711 {
public:
  enum Status : uint8_t {
    OK,              // bacaan terbaru valid
    BELUM_SIAP,      // baru mulai/bangun/ganti gain: menunggu settling
    TIDAK_TERHUBUNG, // HX711 tidak mengirim data (kabel DT/SCK/VCC)
    DI_LUAR_RENTANG, // ADC jenuh: beban terlalu berat atau kabel load cell salah
    TIDUR            // dimatikan dengan tidur()
  };

  // pinData = DT/DOUT, pinClock = SCK/PD_SCK.
  TimbanganHX711(uint8_t pinData, uint8_t pinClock)
      : _dt(pinData), _sck(pinClock), _pulsa(0), _status(BELUM_SIAP), _kalibrasi(0), _adaHalus(0),
        _settle(0), _halusN(1) {}
  // Tanpa pin: bacaan mentah dikirim sendiri lewat perbarui(long).
  TimbanganHX711() : TimbanganHX711(TANPA_PIN, TANPA_PIN) {}

  // Panggil di setup(). Tidak menunggu; bacaan pertama siap setelah settling (±0,5 s).
  void mulai();
  // Panggil di setiap loop(). true jika ada berat baru yang valid.
  bool perbarui();
  // Untuk ADC lain atau pengujian: kirim bacaan mentah 24 bit sendiri.
  bool perbarui(long mentah);

  // --- Bacaan ---
  // Berat dalam satuan beban kalibrasi (biasanya gram). NAN jika belum siap
  // atau sedang tara/kalibrasi.
  float berat() const;
  // Nilai ADC setelah penghalusan, sebelum tara dan faktor (untuk diagnosa).
  long mentah() const { return lroundf(_halus); }
  bool siap() const { return _status == OK; }
  Status status() const { return (Status)_status; }
  const __FlashStringHelper *alasan() const; // penjelasan status + saran

  // --- Tara & kalibrasi (berjalan di latar belakang lewat perbarui()) ---
  // Nol-kan timbangan: rata-rata dari sampel berikutnya menjadi titik nol.
  void tara(uint8_t sampel = 10);
  // Taruh beban yang beratnya diketahui (mis. 500 gram) lalu panggil ini.
  // Panggil tara() dulu tanpa beban.
  void kalibrasi(float bebanDiketahui, uint8_t sampel = 10);
  bool sedangMengukur() const { return _target; } // tara/kalibrasi belum selesai

  // Simpan nilai ini (EEPROM/Preferences) agar tidak perlu kalibrasi ulang.
  long offset() const { return _offset; }
  float faktor() const { return _faktor; }
  void aturOffset(long offset) { _offset = offset; }
  void aturFaktor(float faktor) { _faktor = faktor; } // bacaan mentah per satuan berat

  // --- Pengaturan ---
  // Penghalusan: rata-rata kira-kira 2^n sampel terakhir (0 = mati, default 1 = ±2 sampel).
  // Makin besar makin tenang, tapi makin lambat mengikuti beban baru.
  void aturPenghalusan(uint8_t n) { _halusN = n > 7 ? 7 : n; }
  // 128 (kanal A, default), 64 (kanal A), atau 32 (kanal B).
  void aturGain(uint8_t gain);
  void tidur();  // hemat daya: HX711 dan load cell dimatikan
  void bangun(); // kembali normal, lalu settling lagi

private:
  enum : uint8_t { TANPA_PIN = 0xFF };

  bool baca24(long &hasil);
  void mulaiSettling();

  // 19 byte di AVR: status kecil dipadatkan ke bitfield.
  long _offset = 0;
  float _faktor = 1;      // saat kalibrasi: berisi beban yang diketahui
  float _halus = 0;       // bacaan setelah penghalusan; saat tara/kalibrasi: rata-rata berjalan
  uint8_t _dt, _sck;
  uint8_t _waktu = 0;     // millis() / 8, 8 bit: saat data terakhir / awal settling
  uint8_t _hitung = 0, _target = 0; // sampel tara/kalibrasi yang sudah / harus diambil
  uint8_t _pulsa : 2;     // pulsa clock per bacaan - 25: 0 = A/128, 1 = B/32, 2 = A/64
  uint8_t _status : 3;
  uint8_t _kalibrasi : 1; // pengukuran yang berjalan: 0 = tara, 1 = kalibrasi
  uint8_t _adaHalus : 1;  // _halus sudah berisi bacaan
  uint8_t _settle : 3;    // bacaan yang masih dibuang (settling)
  uint8_t _halusN : 3;
};
