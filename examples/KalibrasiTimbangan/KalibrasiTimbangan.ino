// Kalibrasi timbangan lewat Serial Monitor (115200, akhiran baris "Newline").
// 1. Kosongkan timbangan, ketik t lalu Enter (tara).
// 2. Taruh beban yang beratnya diketahui (mis. botol air 500 gram),
//    ketik beratnya dalam gram, mis. 500, lalu Enter.
// 3. Catat FAKTOR yang dicetak dan isikan ke contoh DasarTimbangan.
#include <TimbanganHX711.h>

TimbanganHX711 timbangan(3, 2); // DT, SCK
bool menunggu = false;

void setup() {
  Serial.begin(115200);
  timbangan.mulai();
  Serial.println(F("Kosongkan timbangan, ketik t lalu Enter."));
}

void loop() {
  timbangan.perbarui();

  if (Serial.available()) {
    String masuk = Serial.readStringUntil('\n');
    masuk.trim();
    if (masuk == "t") {
      timbangan.tara();
      Serial.println(F("Tara... jangan sentuh timbangan."));
    } else if (masuk.toFloat() > 0) {
      timbangan.kalibrasi(masuk.toFloat());
      Serial.println(F("Kalibrasi... jangan sentuh timbangan."));
    }
    menunggu = true;
  }

  if (menunggu && !timbangan.sedangMengukur()) {
    menunggu = false;
    Serial.print(F("OFFSET = "));
    Serial.print(timbangan.offset());
    Serial.print(F(", FAKTOR = "));
    Serial.println(timbangan.faktor(), 4);
    Serial.println(F("Taruh beban yang diketahui, ketik beratnya (gram) lalu Enter."));
  }
}
