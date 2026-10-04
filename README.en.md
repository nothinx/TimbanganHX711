# TimbanganHX711

[Bahasa Indonesia](README.md)

Arduino library (Indonesian API) for a **digital scale with an HX711 and a load cell**. `perbarui()` (update) never makes the program wait, tare and calibration run in the background, and a disconnected sensor is reported with a clear message.

```cpp
#include <TimbanganHX711.h>

TimbanganHX711 scale(3, 2); // DT, SCK

void setup() {
  Serial.begin(115200);
  scale.mulai();           // begin, never blocks
  scale.aturFaktor(420.0); // setFactor, from the KalibrasiTimbangan example
  scale.tara();            // tare in the background
}

void loop() {
  if (scale.perbarui()) Serial.println(scale.berat()); // weight in grams
}
```

## Why

Measured on an Arduino Uno (cycle-accurate simavr), same sketch for every library. Versions: bogde/HX711 0.7.5, olkal/HX711_ADC 1.2.12, RobTillaart/HX711 0.6.5.

| | **TimbanganHX711** | bogde HX711 | HX711_ADC | RobTillaart HX711 |
|---|---|---|---|---|
| Minimal example lines to grams | **15** | 18 (raw value only) | 67 | 37 |
| Waits for the HX711 | **never** | `read()` waits, forever if unplugged | no | `read()` waits; `begin()` hangs if no HX711 |
| Tare / calibration | **background** | blocks ~1 s | `start()` blocks | blocks ~1 s |
| One 24-bit read | **1,304 cycles (82 µs)** | 6,434 (402 µs) | 6,551 (409 µs) | 6,149 (384 µs) |
| Longest interrupts-off | **< 1 µs** (one pulse) | ~400 µs | not disabled: an ISR > 60 µs can power the HX711 down | ~380 µs |
| Unplugged / saturated | **status + message** | not detected | not detected | `is_ready()` can be true when unplugged |
| RAM per object | 19 B | **13 B** | 135 B | 23 B |

bogde uses 6 bytes less because it blocks; a non-blocking library has to keep the last reading. On AVR the library reads through port registers (5× faster than `digitalWrite`/`digitalRead`); other boards use `digitalWrite` with 1 µs delays to meet the datasheet timing.

## Function reference

| Indonesian | English | Notes |
|---|---|---|
| `TimbanganHX711(pinDT, pinSCK)` | constructor | |
| `mulai()` | begin | non-blocking |
| `perbarui()` | update | `true` when a new valid weight is available |
| `perbarui(long raw)` | update(raw) | feed readings from another ADC or a test |
| `berat()` | weight | in the calibration unit; `NAN` if not ready or taring |
| `mentah()` | raw | smoothed ADC value before tare and factor |
| `siap()` | ready | |
| `status()` | status | `OK`, `BELUM_SIAP` (settling), `TIDAK_TERHUBUNG` (not connected), `DI_LUAR_RENTANG` (saturated), `TIDUR` (asleep) |
| `alasan()` | reason | Indonesian explanation with a fix |
| `tara(samples)` | tare | background, default 10 samples |
| `kalibrasi(knownWeight, samples)` | calibrate | background |
| `sedangMengukur()` | isMeasuring | tare/calibration running |
| `offset()`, `faktor()` | offset, factor | save these (EEPROM) |
| `aturOffset()`, `aturFaktor()` | setOffset, setFactor | load saved values |
| `aturPenghalusan(n)` | setSmoothing | average of ~2^n samples, default 1 |
| `aturGain(gain)` | setGain | 128, 64, or 32 (channel B) |
| `tidur()`, `bangun()` | sleep, wake | |

## Examples

`DasarTimbangan` (basic scale), `CekSambungan` (wiring check), `KalibrasiTimbangan` (calibration over Serial), `SimpanKalibrasi` (store calibration in EEPROM / ESP32 Preferences).

## Status

Version 1.0.0 passes 12 PC logic tests with a pin-level HX711 emulator that follows the datasheet, the AVR port-register path is tested in a simulator, and it compiles on 7 boards. It has **not yet been tested with a real HX711 and load cell**.

## License

MIT © 2026 Amadeo Wisesa.
