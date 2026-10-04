// Simpan hasil kalibrasi agar tidak hilang saat listrik mati.
// AVR (Uno, Nano, Mega): EEPROM. ESP32: Preferences (flash).
// Ketik t lalu Enter untuk tara, atau berat beban (gram) lalu Enter untuk kalibrasi;
// hasilnya langsung disimpan dan dipakai lagi saat board dinyalakan ulang.
#include <TimbanganHX711.h>
#if defined(ESP32)
#include <Preferences.h>
Preferences simpanan;
#elif defined(__AVR__)
#include <EEPROM.h>
#endif

TimbanganHX711 timbangan(3, 2); // DT, SCK
bool menunggu = false;

void simpan() {
#if defined(ESP32)
  simpanan.putLong("offset", timbangan.offset());
  simpanan.putFloat("faktor", timbangan.faktor());
#elif defined(__AVR__)
  EEPROM.put(0, timbangan.offset());
  EEPROM.put(4, timbangan.faktor());
#endif
}

void muat() {
  long offset = 0;
  float faktor = NAN;
#if defined(ESP32)
  simpanan.begin("timbangan");
  offset = simpanan.getLong("offset", 0);
  faktor = simpanan.getFloat("faktor", NAN);
#elif defined(__AVR__)
  EEPROM.get(0, offset);
  EEPROM.get(4, faktor);
#endif
  if (isnan(faktor) || faktor == 0) { // belum pernah dikalibrasi (EEPROM kosong = NAN)
    Serial.println(F("Belum ada kalibrasi tersimpan."));
    return;
  }
  timbangan.aturOffset(offset);
  timbangan.aturFaktor(faktor);
  Serial.println(F("Kalibrasi dimuat."));
}

void setup() {
  Serial.begin(115200);
  timbangan.mulai();
  muat();
}

void loop() {
  if (timbangan.perbarui() && !menunggu) {
    Serial.print(timbangan.berat(), 1);
    Serial.println(F(" gram"));
  }
  if (Serial.available()) {
    String masuk = Serial.readStringUntil('\n');
    masuk.trim();
    if (masuk == "t") timbangan.tara();
    else if (masuk.toFloat() > 0) timbangan.kalibrasi(masuk.toFloat());
    menunggu = true;
  }
  if (menunggu && !timbangan.sedangMengukur()) {
    menunggu = false;
    simpan();
    Serial.println(F("Disimpan."));
  }
}
