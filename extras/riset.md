# Riset TimbanganHX711

Dikumpulkan 4 Oktober 2026 sebelum menulis kode (lihat `alat/SPEK-SENSOR.md` di repo katalog).

## Datasheet

HX711, 24-Bit ADC for Weigh Scales, Avia Semiconductor:
<https://cdn.sparkfun.com/datasheets/Sensors/ForceFlex/hx711_english.pdf> (cermin resmi SparkFun; situs Avia sudah tidak aktif).

| Parameter | Nilai | Halaman | Dipakai di kode |
|---|---|---|---|
| DT HIGH = data belum siap, LOW = siap | - | 4 | `perbarui()` hanya membaca saat DT LOW, tidak pernah menunggu |
| Bit per bacaan | 24, MSB dulu, komplemen dua | 4 | `baca24()`, perluasan tanda |
| Pulsa SCK per bacaan | 25 = A/128, 26 = B/32, 27 = A/64 (untuk konversi **berikutnya**) | 4, tabel 3 | `aturGain()` |
| Pulsa ke-25 menarik DT HIGH | - | 4 | DT tetap LOW setelah membaca = bukan HX711 (`TIDAK_TERHUBUNG`) |
| Data jenuh | 800000h (min), 7FFFFFh (maks) | 3-4 | `DI_LUAR_RENTANG` |
| SCK tinggi (T3) | 0,2 / 1 / 50 µs (min / tipikal / maks) | 5 | interrupt dimatikan hanya selama pulsa tinggi; `nop` di AVR, `delayMicroseconds(1)` di MCU cepat |
| SCK rendah (T4) | min 0,2 µs | 5 | `delayMicroseconds(1)` di MCU cepat |
| Data valid setelah tepi naik (T2) | maks 0,1 µs | 5 | bit dibaca setelah pulsa turun |
| SCK tinggi > 60 µs | power down; SCK LOW lagi = reset ke A/128 | 5 | `tidur()`, `bangun()` |
| Settling (power up, reset, ganti kanal/gain) | 400 ms (10 SPS), 50 ms (80 SPS) = 4 periode | 3 | buang 5 bacaan pertama (4 + bacaan yang mengirim gain baru) |
| Kecepatan data | 10 SPS (RATE = 0) atau 80 SPS (RATE = 1) | 3-4 | batas tanpa data 500 ms (5 periode 10 SPS) |

Catatan modul di pasaran: modul HX711 hijau yang umum menyambungkan RATE ke GND (10 SPS); untuk 80 SPS pin RATE harus dipindah ke VCC (memotong jalur di PCB).

## Pesaing

Diunduh dari rilis resminya: bogde/HX711 0.7.5 (991 bintang), olkal/HX711_ADC 1.2.12, RobTillaart/HX711 0.6.5.

| Temuan (dari source) | bogde | HX711_ADC | RobTillaart |
|---|---|---|---|
| Menunggu data | `read()` menunggu DT LOW **tanpa batas waktu**; `tare()`/`get_units(10)` ±1 s | `update()` non-blocking; `start()` memblokir selama waktu stabil + tara | `read()` menunggu tanpa batas waktu; `begin()` memanggil `reset()` → `read()`: **program macet di setup() jika HX711 tidak tersambung** |
| Interrupt | dimatikan sepanjang 25-27 bit (±400 µs di Uno) | **tidak dimatikan** secara bawaan (`SCK_DISABLE_INTERRUPTS 0`): ISR > 60 µs saat SCK tinggi membuat HX711 power down | dimatikan sepanjang 25-27 bit |
| Waktu | - | `while(millis() < t)`: salah saat `millis()` meluap (issue #51) | - |
| RAM | 13 B | 135 B (buffer 16 sampel + 2) | 23 B |

## Issue yang paling banyak dibahas (GitHub, 4 Okt 2026)

| Issue | Masalah | Penyelesaian di TimbanganHX711 | Diuji |
|---|---|---|---|
| bogde #70 (100 komentar), #20, #8 | Cara kalibrasi ke gram/kg | `tara()` + `kalibrasi(beratDiketahui)`, contoh `KalibrasiTimbangan` interaktif | uji "tara + kalibrasi" |
| bogde #35 (100), RobTillaart #40, #43 | Banyak HX711 sekaligus | objek per modul, tanpa variabel global/static | uji "dua timbangan" |
| bogde #75, RobTillaart #29 | ESP32/ESP8266: clock terlalu cepat | `delayMicroseconds(1)` untuk T3/T4 di MCU selain AVR | uji PC (non-AVR) |
| bogde #67, #73, RobTillaart #29, #58 | ESP8266 reboot (watchdog) karena menunggu | tidak pernah menunggu | uji "tidak terhubung": tiap `perbarui()` < 20 µs |
| bogde #132 (`-8388608`), #131 | Bacaan jenuh tidak dikenali | status `DI_LUAR_RENTANG` + saran cek kabel | uji "ADC jenuh" |
| HX711_ADC #59, RobTillaart #1, #75 | Kalibrasi hilang saat mati listrik | `offset()`/`faktor()` + `aturOffset()`/`aturFaktor()`, contoh `SimpanKalibrasi` (EEPROM/Preferences) | uji "kalibrasi dipakai lagi" |
| HX711_ADC #28, #33, #67, #125 | Tidak mau tara saat menyala | tara tidak otomatis; panggil `tara()` atau muat offset tersimpan | - |
| RobTillaart #65, #26 | `is_ready()` true walau tidak tersambung, bacaan `nan` | DT pakai pull-up (lepas = HIGH); DT tetap LOW setelah pulsa ke-25 = tidak terhubung; `alasan()` | uji "VCC lepas" |
| RobTillaart #70 | Start-up lama | `mulai()` tidak menunggu; settling di latar belakang | uji "settling" |
| HX711_ADC `config.h` baris 41 | ISR > 60 µs saat SCK tinggi membuat HX711 power down (diakui di komentar mereka) | interrupt mati hanya per pulsa; SCK tidak pernah > 60 µs | uji "ISR panjang 120 µs" |
| RobTillaart #27, bogde #12 | Kanal B | `aturGain(32)` | uji "gain 64 dan kanal B" |
