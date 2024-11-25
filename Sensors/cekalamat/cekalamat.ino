#include <Wire.h>

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); // Inisialisasi pin I2C

  Serial.println("Memulai pemindaian I2C...");
  // Tambahkan delay untuk menunggu Serial Monitor terbuka
  delay(1000);

  byte error, address;
  int perangkat = 0;

  for (address = 1; address < 127; address++) {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0) {
      Serial.print("Perangkat I2C ditemukan di alamat 0x");
      if (address < 16) 
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.println(" !");
      perangkat++;
    }
  }

  if (perangkat == 0)
    Serial.println("Tidak ada perangkat I2C yang ditemukan\n");
  else
    Serial.println("Pemindaian selesai\n");
}

void loop() {
  // Kosong
}
