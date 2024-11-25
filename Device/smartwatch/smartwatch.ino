#include <WiFi.h>         // Library untuk Wi-Fi
#include <WebServer.h>    // Library untuk Web Server
#include <TFT_eSPI.h>  // Library TFT_eSPI
TFT_eSPI tft = TFT_eSPI();  // Inisialisasi objek TFT

#include <Adafruit_GFX.h>  // Library GFX
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"
#include "spo2_algorithm.h"

// Konfigurasi Wi-Fi
const char* ssid = "BayMax";           // Ganti dengan SSID WiFi Anda
const char* password = "11111111";   // Ganti dengan password WiFi Anda

WebServer server(80);  // Inisialisasi web server pada port 80

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

// Fungsi untuk menyajikan halaman web
void handleRoot() {
  // HTML + JavaScript untuk halaman web
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Heart Rate Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    /* Gaya global untuk body */
    body {
      font-family: Arial, sans-serif;
      margin: 0;
      padding: 0;
      background-color: #f4f4f9;
      color: #333;
    }

    /* Gaya untuk header */
    header {
      background-color: #007bff;
      color: white;
      padding: 15px 0;
      text-align: center;
      font-size: 24px;
      font-weight: bold;
      box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
    }

    /* Kontainer utama */
    .container {
      max-width: 600px;
      margin: 20px auto;
      padding: 20px;
      background: white;
      border-radius: 10px;
      box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
      text-align: center;
    }

    /* Judul untuk grafik */
    .chart-title {
      margin: 20px 0;
      font-size: 20px;
      font-weight: bold;
      color: #007bff;
    }

    /* Gaya untuk elemen informasi */
    .info {
      margin-top: 20px;
      font-size: 18px;
      color: #555;
    }

    .info span {
      font-weight: bold;
      color: #007bff;
    }
  </style>
  <script src="https://cdn.jsdelivr.net/npm/chart.js"></script>
</head>
<body>
  <header>
    Heart Rate Monitor
  </header>
  <div class="container">
    <div class="chart-title">Live Heart Rate (BPM)</div>
    <canvas id="heartRateChart" width="400" height="200"></canvas>
    <div class="info">
      <p>Current Heart Rate: <span id="currentHR">--</span> BPM</p>
      <p>Last Update: <span id="lastUpdate">--</span></p>
    </div>
  </div>
  <script>
    const ctx = document.getElementById('heartRateChart').getContext('2d');
    const chart = new Chart(ctx, {
      type: 'line',
      data: {
        labels: [],  // Waktu
        datasets: [{
          label: 'Heart Rate (BPM)',
          data: [],   // Data detak jantung
          borderColor: '#007bff',
          backgroundColor: 'rgba(0, 123, 255, 0.2)',
          borderWidth: 2,
          tension: 0.4,
          fill: true,
        }]
      },
      options: {
        responsive: true,
        scales: {
          x: { 
            title: { display: true, text: 'Time (s)' },
            ticks: { color: '#555' }
          },
          y: { 
            title: { display: true, text: 'BPM' },
            min: 0,
            max: 100,
            ticks: { color: '#555' }
          }
        },
        plugins: {
          legend: {
            labels: {
              color: '#333',
              font: { size: 14 }
            }
          }
        }
      }
    });

    // Update data setiap 60 detik
    setInterval(async () => {
      try {
        const response = await fetch('/data');
        const data = await response.json();
        const time = new Date().toLocaleTimeString();

        // Tambahkan data ke grafik
        chart.data.labels.push(time);
        chart.data.datasets[0].data.push(data.beatAvg);

        if (chart.data.labels.length > 50) {
          chart.data.labels.shift();  // Hapus data lama
          chart.data.datasets[0].data.shift();
        }

        chart.update();

        // Perbarui nilai di bawah grafik
        document.getElementById('currentHR').innerText = data.beatAvg;
        document.getElementById('lastUpdate').innerText = time;
      } catch (error) {
        console.error("Error fetching data:", error);
      }
    }, 1000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}


// Fungsi untuk memberikan data JSON detak jantung
void handleData() {
  String json = "{\"beatAvg\": " + String(beatAvg) + "}";
  server.send(200, "application/json", json);
}

void setup() {
  // Inisialisasi serial untuk debugging
  Serial.begin(115200);

  Serial.println("Initializing...");

  // Inisialisasi Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to Wi-Fi");
   while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  Serial.println("\nConnected! IP address: ");
  Serial.println(WiFi.localIP());  // Tampilkan IP lokal ESP32

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

  // Atur handler untuk web server
  server.on("/", handleRoot);    // Halaman utama
  server.on("/data", handleData); // Endpoint JSON
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  
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
    else beatAvg = 0;
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

    // Tampilkan suhu
    tft.setCursor(20, 180);
    tft.print(WiFi.localIP()); // Tampilkan suhu dengan 1 angka desimal
  }
}
