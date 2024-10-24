#include <TFT_eSPI.h>  // Library TFT_eSPI
TFT_eSPI tft = TFT_eSPI();  // Inisialisasi objek TFT

#include <Adafruit_GFX.h>  // Library GFX
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

MAX30105 particleSensor;
const byte RATE_SIZE = 4; // Ukuran untuk averaging detak jantung
byte rates[RATE_SIZE]; // Array untuk menyimpan detak jantung
byte rateSpot = 0;
long lastBeat = 0; // Waktu ketika detak terakhir terjadi
float beatsPerMinute;
int beatAvg;

unsigned long lastUpdate = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

// Deklarasi pin untuk buzzer dan LED
const int buzzerPin = 17;
const int ledPin = 16;
unsigned long alertStartTime = 0; // Waktu saat peringatan dimulai
bool alertActive = false;         // Status apakah peringatan aktif
bool alertOn = false;             // Status apakah buzzer dan LED menyala
unsigned long lastBlinkTime = 0;  // Waktu terakhir berkedip
const unsigned long blinkInterval = 500; // Interval kedip (500ms)

void setup() {
  // Inisialisasi serial untuk debugging
  Serial.begin(115200);

  Serial.println("Initializing...");

  // Inisialisasi sensor MAX30105
  if (!particleSensor.begin(Wire, 400000)) // Gunakan port I2C default dengan kecepatan 400kHz
  {
    Serial.println("MAX30105 tidak ditemukan. Silakan periksa wiring/power.");
    while (1);
  }
  Serial.println("Letakkan jari Anda di sensor dengan tekanan yang stabil.");

  // Konfigurasi sensor
  particleSensor.setup();
  particleSensor.setPulseAmplitudeRed(0x0A); // Nyalakan LED merah dengan intensitas rendah untuk menunjukkan bahwa sensor aktif
  particleSensor.setPulseAmplitudeGreen(0);  // Matikan LED hijau
  particleSensor.enableDIETEMPRDY(); // Aktifkan pembacaan suhu

  // Inisialisasi TFT display
  tft.init();
  tft.setRotation(0);  // Atur orientasi layar
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);

  // Inisialisasi buzzer dan LED sebagai output
  pinMode(buzzerPin, OUTPUT);
  pinMode(ledPin, OUTPUT);

  // Matikan buzzer dan LED di awal
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, LOW);
}

void loop() {
  long irValue = particleSensor.getIR(); // Baca nilai IR
  float temperature = particleSensor.readTemperature(); // Baca suhu

  // Cek apakah ada detak jantung yang terdeteksi
  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 20) {
      rates[rateSpot++] = (byte)beatsPerMinute; // Simpan pembacaan ini di array
      rateSpot %= RATE_SIZE; // Wrap variabel

      // Hitung rata-rata pembacaan
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++)
        beatAvg += rates[x];
      beatAvg /= RATE_SIZE;
    }
  }

  // Periksa apakah beatAvg melebihi 80 dan nyalakan peringatan jika demikian
  if (beatAvg > 80 && !alertActive) {
    alertActive = true;                 // Tandai bahwa peringatan aktif
    alertStartTime = millis();          // Catat waktu mulai peringatan
    lastBlinkTime = millis();           // Reset waktu kedipan
    alertOn = true;                     // Set alert menyala
    Serial.println("Alert! BeatAvg > 80");
  }

  // Jika peringatan aktif, kendalikan kedipan
  if (alertActive) {
    // Cek apakah waktunya untuk berkedip
    if (millis() - lastBlinkTime >= blinkInterval) {
      lastBlinkTime = millis();         // Reset waktu kedipan

      // Toggle status LED dan buzzer
      alertOn = !alertOn;
      digitalWrite(buzzerPin, alertOn ? HIGH : LOW);
      digitalWrite(ledPin, alertOn ? HIGH : LOW);
    }

    // Matikan buzzer & LED setelah 3 detik
    if (millis() - alertStartTime >= 3000) {
      alertActive = false;              // Reset status peringatan
      digitalWrite(buzzerPin, LOW);     // Matikan buzzer
      digitalWrite(ledPin, LOW);        // Matikan LED
      Serial.println("Alert stopped.");
    }
  }

  // Hitung waktu setiap 1 detik
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();
    seconds++;

    if (seconds >= 60) {
      seconds = 0;
      minutes++;
    }

    if (minutes >= 60) {
      minutes = 0;
      hours++;
    }

    if (hours >= 24) {
      hours = 0;
    }

    // Hapus tampilan sebelumnya
    tft.fillScreen(TFT_BLACK);

    // Tampilkan waktu dalam format HH:MM:SS
    tft.setTextSize(4);
    tft.setCursor(20, 50);
    
    if (hours < 10) tft.print("0");
    tft.print(hours);
    tft.print(":");
    if (minutes < 10) tft.print("0");
    tft.print(minutes);
    tft.print(":");
    if (seconds < 10) tft.print("0");
    tft.print(seconds);

    // Tampilkan detak jantung rata-rata
    tft.setTextSize(2); // Ukuran teks lebih kecil untuk beatAvg
    tft.setCursor(20, 120);
    tft.print("HR: ");
    tft.print(beatAvg);
    tft.print(" BPM");

    // Tampilkan suhu
    tft.setCursor(20, 150);
    tft.print("Temp: ");
    tft.print(temperature, 1); // Tampilkan suhu dengan 1 angka desimal
    tft.print(" C");
  }
}
