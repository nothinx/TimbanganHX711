# TimbanganHX711

[English](README.en.md)

Library Arduino berbahasa Indonesia untuk **timbangan digital dengan HX711 dan load cell**. `perbarui()` tidak pernah membuat program menunggu, tara dan kalibrasi berjalan di latar belakang, dan kabel yang lepas langsung dilaporkan dengan pesan yang jelas.

```cpp
if (timbangan.perbarui()) Serial.println(timbangan.berat());
```

## Rapor dibanding library lain

Diukur di Arduino Uno (simulator simavr, akurat per siklus) dengan sketch yang sama untuk semua library. Versi pesaing: bogde/HX711 0.7.5, olkal/HX711_ADC 1.2.12, RobTillaart/HX711 0.6.5.

| | **TimbanganHX711** | bogde HX711 | HX711_ADC | RobTillaart HX711 |
|---|---|---|---|---|
| Baris contoh minimal sampai berat dalam gram | **15** | 18 (hanya angka mentah) | 67 | 37 |
| Program menunggu saat HX711 belum siap | **tidak pernah** | `read()` menunggu, tanpa batas jika kabel lepas | tidak | `read()` menunggu; `begin()` macet jika HX711 tidak tersambung |
| Tara / kalibrasi | **di latar belakang** | memblokir ±1 detik | `start()` memblokir | memblokir ±1 detik |
| Satu bacaan 24 bit | **1.304 siklus (82 µs)** | 6.434 (402 µs) | 6.551 (409 µs) | 6.149 (384 µs) |
| Interrupt dimatikan paling lama | **< 1 µs** (satu pulsa) | ±400 µs (seluruh bacaan) | tidak dimatikan: ISR > 60 µs bisa membuat HX711 mati | ±380 µs |
| Kabel lepas / data jenuh | **status + pesan Indonesia** | tidak dideteksi | tidak dideteksi | `is_ready()` bisa true walau lepas (issue #65) |
| RAM per objek | 19 B | **13 B** | 135 B | 23 B |

Kenapa RAM kita 6 byte lebih besar dari bogde: bogde memblokir, jadi tidak menyimpan apa pun di antara pemanggilan. Library non-blocking harus menyimpan bacaan terakhir (4 B); sisanya untuk tara di latar belakang (2 B) dan deteksi kabel lepas (1 B). Sketch bogde yang dibuat non-blocking dengan `is_ready()` juga perlu menyimpan bacaan terakhir sendiri.

## Fitur

- **Tidak pernah menunggu.** Data dibaca hanya jika HX711 sudah siap, tanpa `delay()` dan tanpa loop menunggu.
- **Aman untuk interrupt.** Interrupt hanya dimatikan selama satu pulsa clock (< 1 µs di Uno), sehingga Serial, encoder, dan servo tetap berjalan, dan pulsa SCK tidak pernah melewati batas 60 µs yang membuat HX711 mati (datasheet hlm. 5).
- **Status jelas:** `OK`, `BELUM_SIAP`, `TIDAK_TERHUBUNG`, `DI_LUAR_RENTANG`, `TIDUR`, dan `alasan()` berisi saran perbaikan.
- **Tara dan kalibrasi di latar belakang**, hasilnya bisa disimpan ke EEPROM atau Preferences ESP32.
- Settling otomatis (bacaan awal yang belum stabil dibuang), gain 128/64 dan kanal B, mode tidur, banyak timbangan sekaligus.
- Di Uno membaca lewat register port langsung, 5× lebih cepat dari `digitalWrite`/`digitalRead`.

## Board yang didukung

| Board | Catatan |
|---|---|
| Arduino Uno / Nano / Mega | jalur register port cepat |
| ESP32, ESP32-C3, ESP32-S3 | `delayMicroseconds(1)` per pulsa agar sesuai batas waktu datasheet |
| STM32 Blackpill F411, Bluepill F103 | sama dengan ESP32 |

HX711 bekerja di 2,6–5,5 V: sambungkan VCC modul ke tegangan yang sama dengan board (5 V untuk Uno, 3,3 V untuk ESP32/STM32).

## Instalasi

Arduino IDE: **Sketch → Include Library → Manage Libraries**, cari `TimbanganHX711`, klik **Install**.

## Sambungan

| HX711 | Arduino Uno | ESP32 |
|---|---|---|
| VCC | 5V | 3V3 |
| GND | GND | GND |
| DT | pin 3 | pin mana saja |
| SCK | pin 2 | pin mana saja |

Load cell 4 kabel ke HX711: merah E+, hitam E-, putih A-, hijau A+. Warna bisa berbeda antar pabrik; jika berat terbaca negatif saat ditekan, tukar A+ dan A-.

## Contoh cepat

```cpp
#include <TimbanganHX711.h>

TimbanganHX711 timbangan(3, 2); // DT, SCK
const float FAKTOR = 420.0;     // dari contoh KalibrasiTimbangan

void setup() {
  Serial.begin(115200);
  timbangan.mulai();
  timbangan.aturFaktor(FAKTOR);
  timbangan.tara(); // tanpa beban saat menyala = nol
}

void loop() {
  if (timbangan.perbarui()) {
    Serial.print(timbangan.berat(), 1);
    Serial.println(" gram");
  }
}
```

`perbarui()` bernilai `true` hanya jika ada berat baru yang valid: tidak saat settling, tara, kalibrasi, atau kabel lepas.

## Kalibrasi

1. Jalankan contoh `KalibrasiTimbangan`, buka Serial Monitor 115200 dengan akhiran baris "Newline".
2. Kosongkan timbangan, ketik `t` lalu Enter.
3. Taruh benda yang beratnya diketahui (mis. botol air 500 gram, ditimbang dulu di timbangan dapur), ketik `500` lalu Enter.
4. Isikan `FAKTOR` yang dicetak ke program kamu, atau pakai contoh `SimpanKalibrasi` agar tersimpan otomatis.

Satuan berat mengikuti satuan beban kalibrasi: kalibrasi dengan `0.5` (kg) membuat `berat()` dalam kilogram.

## Gejala → penyebab → solusi

| Gejala | Penyebab | Solusi |
|---|---|---|
| `alasan()`: HX711 tidak mengirim data | kabel VCC/GND/DT/SCK lepas, atau nomor pin di program tertukar | cek kabel; jalankan contoh `CekSambungan` |
| `alasan()`: bacaan jenuh | beban melebihi kapasitas load cell, atau kabel load cell salah | cek kapasitas dan kabel E+, E-, A+, A- |
| Berat negatif saat ditekan | A+ dan A- tertukar | tukar kabelnya, atau kalibrasi ulang (faktor jadi negatif, tetap benar) |
| Berat bergeser pelan (drift) | load cell masih menyesuaikan suhu, baut longgar | tunggu beberapa menit setelah menyala; kencangkan dudukan; `tara()` ulang |
| Angka melompat-lompat | getaran, kabel panjang tanpa pelindung | `aturPenghalusan(3)`; pendekkan kabel load cell |
| Serial/servo tersendat saat menimbang | library lain mematikan interrupt ±400 µs per bacaan | tidak terjadi di library ini (< 1 µs) |

## Referensi fungsi

| Fungsi | Keterangan |
|---|---|
| `TimbanganHX711(pinDT, pinSCK)` | Buat objek. |
| `TimbanganHX711()` | Tanpa pin, untuk `perbarui(long)` dari ADC lain. |
| `void mulai()` | Panggil di `setup()`. Tidak menunggu. |
| `bool perbarui()` | Panggil di setiap `loop()`. `true` jika ada berat baru yang valid. |
| `bool perbarui(long mentah)` | Kirim bacaan 24 bit sendiri (ADC lain, pengujian). |
| `float berat()` | Berat dalam satuan kalibrasi. `NAN` jika belum siap atau sedang tara/kalibrasi. |
| `long mentah()` | Nilai ADC setelah penghalusan, sebelum tara dan faktor. |
| `bool siap()` | Bacaan terbaru valid. |
| `Status status()` | `OK`, `BELUM_SIAP`, `TIDAK_TERHUBUNG`, `DI_LUAR_RENTANG`, `TIDUR`. |
| `alasan()` | Penjelasan status dan saran, bisa langsung `Serial.println()`. |
| `void tara(sampel = 10)` | Nol-kan timbangan dari rata-rata sampel berikutnya. |
| `void kalibrasi(beratDiketahui, sampel = 10)` | Hitung faktor dari beban yang diketahui. |
| `bool sedangMengukur()` | Tara/kalibrasi belum selesai. |
| `long offset()`, `float faktor()` | Hasil tara dan kalibrasi, untuk disimpan. |
| `void aturOffset(long)`, `void aturFaktor(float)` | Muat hasil yang tersimpan. |
| `void aturPenghalusan(n)` | Rata-rata ±2ⁿ sampel (0–7, default 1). |
| `void aturGain(gain)` | 128 (default), 64, atau 32 (kanal B). |
| `void tidur()`, `void bangun()` | Hemat daya: HX711 dan load cell dimatikan. |

## Contoh yang tersedia

| Contoh | Isi |
|---|---|
| `DasarTimbangan` | Timbangan paling sederhana dalam gram. |
| `CekSambungan` | Diagnosa kabel: status, saran, dan bacaan mentah setiap detik. |
| `KalibrasiTimbangan` | Tara dan kalibrasi lewat Serial Monitor. |
| `SimpanKalibrasi` | Kalibrasi tersimpan di EEPROM (AVR) atau Preferences (ESP32). |

## Spesifikasi sensor

Dari datasheet HX711 (Avia Semiconductor, [PDF](https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf)): ADC 24 bit, kanal A gain 128 (±20 mV pada 5 V) atau 64 (±40 mV), kanal B gain 32 (±80 mV), 10 atau 80 sampel per detik, tegangan 2,6–5,5 V, arus < 1,5 mA (tidur < 1 µA). Modul hijau yang umum dijual memakai 10 sampel per detik (pin RATE ke GND). Parameter yang dipakai kode dan nomor halamannya ada di [`extras/riset.md`](extras/riset.md).

## Kecepatan & memori

Diukur dengan simavr (simulator ATmega328P yang akurat per siklus), Uno 16 MHz. Simavr tidak punya HX711, jadi pin DT dipaksa HIGH (belum siap) atau LOW (satu bacaan 24 bit penuh). Kebenaran jalur register port diuji terpisah dengan HX711 tiruan yang berjalan di AVR yang sama (interrupt pin-change di SCK).

| | Siklus | µs |
|---|---|---|
| `perbarui()` saat data belum siap (paling sering) | 144 | 9 |
| `perbarui()` membaca 24 bit | 1.304 | 82 |
| Interrupt mati per pulsa | ±15 | < 1 |

Pesaing pada skenario yang sama: cek belum siap 105 siklus (bogde, RobTillaart `is_ready()`, tanpa deteksi kabel lepas) dan 205 (HX711_ADC). Selisih 39 siklus dengan bogde adalah pembacaan `millis()` untuk mendeteksi kabel lepas.

`perbarui()` O(1) waktu dan memori. Mengulang pengukuran: sketch `extras/benchmark/TimbanganHX711Benchmark` (butuh simavr).

## Pengujian

Logika diuji otomatis di PC dengan HX711 tiruan di level pin yang mengikuti datasheet (timing pulsa, power down > 60 µs, settling, jenuh, kanal/gain): settling, tara + kalibrasi, kabel lepas sejak awal, dicabut lalu dipasang lagi, VCC lepas, jenuh, ISR 120 µs di tengah pembacaan, gain 64 dan kanal B, tidur/bangun, luapan `millis()`, dua timbangan, dan jalur tanpa pin.

```sh
cd extras/test
g++ -std=c++11 -I. -I../../src uji.cpp ../../src/TimbanganHX711.cpp -o uji && ./uji
```

## Status

Versi 1.0.0 lolos uji logika di PC (12 kasus), jalur register port AVR lolos uji di simulator, dan compile di 7 board. **Belum diuji dengan HX711 dan load cell sungguhan.** Jika kamu mencobanya, laporan hasilnya (board, load cell, beban acuan, galat) sangat membantu: buka *issue* di GitHub.

## Lisensi

MIT © 2026 Amadeo Wisesa. Lihat [LICENSE](LICENSE).
