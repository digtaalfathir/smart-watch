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
float temperature;

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

//button
int displayMode = 0; // 0 untuk waktu, 1 untuk detak jantung
const int buttonPin = 0; // Pin untuk tombol
bool buttonPressed = false;

//delay
unsigned long lastUpdateTime = 0;
const unsigned long updateInterval = 1000; // Update setiap 1 detik

void checkButton() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(buttonPin);

  if (lastButtonState == HIGH && currentButtonState == LOW) { // Tombol ditekan
    displayMode = (displayMode + 1) % 2; // Ganti mode (0 -> 1 -> 0)
    delay(200); // Debounce
  }

  lastButtonState = currentButtonState;
}


void updateClock() {
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
  }
}

void updateHeartRate() {
  long irValue = particleSensor.getIR(); // Baca nilai IR
  temperature = particleSensor.readTemperature(); // Baca suhu

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
}

void displayClock() {
  tft.fillScreen(TFT_BLACK);

  // Warna latar belakang dan elemen
  uint16_t bgColor = TFT_BLACK;
  uint16_t textColor = TFT_WHITE;
  uint16_t accentColor = TFT_CYAN;

  // Ambil hari dan tanggal
  String days[] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
  String months[] = {"Jan", "Feb", "Mar", "Apr", "Mei", "Jun", "Jul", "Agu", "Sep", "Okt", "Nov", "Des"};
  int currentDay = 3; // Ganti dengan pengambilan data hari otomatis jika tersedia
  int currentDate = 17; // Ganti dengan tanggal otomatis
  int currentMonth = 11; // Ganti dengan bulan otomatis (0 = Jan, 11 = Des)

  // Tampilkan waktu utama (HH:MM)
  tft.setTextColor(textColor, bgColor);
  tft.setTextSize(8);
  tft.setCursor(20, 75);
  if (hours < 10) tft.print("0");
  tft.print(hours);
  tft.print(":");
  if (minutes < 10) tft.print("0");
  tft.print(minutes);

  // Garis pemisah dekoratif
  tft.drawLine(20, 155, 220, 155, accentColor);

  // Hari dan tanggal
  tft.setTextSize(2);
  tft.setTextColor(TFT_WHITE, bgColor);
  tft.setCursor(45, 165);
  tft.print(days[currentDay]); // Nama hari
  tft.print(", ");
  tft.print(currentDate);
  tft.print(" ");
  tft.print(months[currentMonth]); // Nama bulan

  // Tampilkan suhu dengan ikon
  tft.setTextSize(2);
  tft.setTextColor(TFT_GREEN, bgColor);
  tft.setCursor(80, 195);
  tft.print(temperature, 1); // Suhu dalam 1 desimal
  tft.print(" C");
}

void displayHeartRate() {
  tft.fillScreen(TFT_BLACK);

  // Gambar hatixx
  int heartColor = (beatAvg > 100) ? TFT_RED : TFT_GREEN; // Warna hati dinamis
  tft.fillCircle(105, 80, 40, heartColor); // Hati kiri atas (x: 120 -> 100)
  tft.fillCircle(145, 80, 40, heartColor); // Hati kanan atas (x: 160 -> 140)
  tft.fillTriangle(65, 80, 180, 80, 120, 160, heartColor); // Bagian bawah hati (x: 80, 200, 140 -> 60, 180, 120)

  // BPM besar
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(6);
  tft.setCursor(40, 180);
  tft.print(beatAvg);
  tft.print(" BPM");

  // Label kecil "Heart Rate"
  tft.setTextSize(2);
  tft.setCursor(60, 230);
  tft.print("Heart Rate");
}

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

  pinMode(buttonPin, INPUT_PULLUP);

  // Matikan buzzer dan LED di awal
  digitalWrite(buzzerPin, LOW);
  digitalWrite(ledPin, LOW);
}

void loop() {
  if (millis() - lastUpdateTime > updateInterval) {
    lastUpdateTime = millis();

    // Perbarui data
    updateClock();
    updateHeartRate();

    // Render tampilan sesuai mode
    if (displayMode == 0) {
      displayClock();
    } else if (displayMode == 1) {
      displayHeartRate();
    }
  }

  // Periksa input tombol tanpa delay
  checkButton();
}
