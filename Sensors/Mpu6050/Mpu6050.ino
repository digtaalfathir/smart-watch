#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  
  // Inisialisasi sensor MPU6050
  mpu.initialize();
  if (!mpu.testConnection()) {
    Serial.println("Tidak dapat menghubungkan ke MPU6050");
    while (1);
  }
  Serial.println("MPU6050 berhasil diinisialisasi.");
}

void loop() {
  // Mengambil nilai accelerometer dan gyroscope
  int16_t ax, ay, az;
  int16_t gx, gy, gz;

  // Membaca accelerometer
  mpu.getAcceleration(&ax, &ay, &az);
  Serial.print("Accelerometer: ");
  Serial.print("X = "); Serial.print(ax);
  Serial.print(", Y = "); Serial.print(ay);
  Serial.print(", Z = "); Serial.println(az);

  // Membaca gyroscope
  mpu.getRotation(&gx, &gy, &gz);
  Serial.print("Gyroscope: ");
  Serial.print("X = "); Serial.print(gx);
  Serial.print(", Y = "); Serial.print(gy);
  Serial.print(", Z = "); Serial.println(gz);

  delay(500); // jeda setengah detik
}
