// Diagnosa kabel HX711. Buka Serial Monitor 115200. Program mencetak status
// setiap detik beserta saran perbaikannya, dan bacaan mentah saat sudah benar.
//
// Bacaan mentah normal tanpa beban: ribuan sampai ratusan ribu, berubah sedikit-sedikit.
// Saat load cell ditekan, angkanya harus berubah jelas. Jika malah turun, tukar kabel A+ dan A-.
#include <TimbanganHX711.h>

const uint8_t PIN_DT = 3, PIN_SCK = 2;
TimbanganHX711 timbangan(PIN_DT, PIN_SCK);
uint32_t waktuCetak = 0;

void setup() {
  Serial.begin(115200);
  timbangan.mulai();
  Serial.println(F("Cek sambungan HX711"));
  Serial.print(F("DT di pin "));
  Serial.print(PIN_DT);
  Serial.print(F(", SCK di pin "));
  Serial.println(PIN_SCK);
}

void loop() {
  timbangan.perbarui();
  if (millis() - waktuCetak < 1000) return;
  waktuCetak = millis();
  if (timbangan.siap()) {
    Serial.print(F("OK, bacaan mentah: "));
    Serial.println(timbangan.mentah());
  } else {
    Serial.println(timbangan.alasan());
  }
}
