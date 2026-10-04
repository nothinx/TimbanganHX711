// Benchmark TimbanganHX711 di ATmega328P 16 MHz (simavr). Cara menjalankan dan
// angka hasilnya: README bagian "Kecepatan & memori".
// Siklus.h: Timer1 tanpa prescaler, UKUR(nama, ulang, kode) mencetak
// "BENCH nama siklus_per_panggilan". simavr tidak punya HX711, jadi pin DT
// dipaksa HIGH (data belum siap) atau LOW (data siap: satu bacaan 24 bit penuh).
#include <TimbanganHX711.h>
#include "Siklus.h"

const uint8_t PIN_DT = 3, PIN_SCK = 2;
TimbanganHX711 timbangan(PIN_DT, PIN_SCK);
volatile bool hasil;

void setup() {
  Serial.begin(115200);
  timbangan.mulai();
  Serial.print(F("BENCH sizeof "));
  Serial.println(sizeof(TimbanganHX711));
  pinMode(PIN_DT, OUTPUT);
  digitalWrite(PIN_DT, HIGH);
  UKUR("perbarui_belum_siap", 1000, hasil = timbangan.perbarui());
  digitalWrite(PIN_DT, LOW);
  UKUR("perbarui_baca_24_bit", 100, hasil = timbangan.perbarui());
  // Interrupt hanya mati selama satu pulsa: digitalWrite HIGH lalu LOW.
  UKUR("interrupt_mati_per_pulsa", 1000, { noInterrupts(); digitalWrite(PIN_SCK, HIGH); digitalWrite(PIN_SCK, LOW); interrupts(); });
  selesai();
}

void loop() {}
