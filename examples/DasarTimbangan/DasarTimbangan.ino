// Timbangan paling sederhana. Isi FAKTOR dengan hasil contoh KalibrasiTimbangan.
// Sambungan HX711: VCC ke 5V, GND ke GND, DT ke pin 3, SCK ke pin 2.
#include <TimbanganHX711.h>

TimbanganHX711 timbangan(3, 2); // DT, SCK
const float FAKTOR = 420.0;     // ganti dengan hasil kalibrasi

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
