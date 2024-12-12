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

const char* ap_ssid = "EBDGuard"; // Nama Wi-Fi yang akan dibuat ESP32
const char* ap_password = "12345678";  // Password Wi-Fi

WebServer server(80);  // Inisialisasi web server pada port 80

MAX30105 particleSensor;
const byte RATE_SIZE = 4; // Ukuran untuk averaging detak jantung
byte rates[RATE_SIZE]; // Array untuk menyimpan detak jantung
byte rateSpot = 0;
long lastBeat = 0; // Waktu ketika detak terakhir terjadi
float beatsPerMinute;
int beatAvg;
float temperature;
int limitHeartRate = 50;

const int AVG_BUFFER_SIZE = 5;  // Ukuran buffer (5 detik)
float beatAvgBuffer[AVG_BUFFER_SIZE];  // Buffer untuk nilai beatAvg
int bufferIndex = 0;  // Indeks buffer
int displayedBeatAvg = 0;  // Rata-rata beatAvg yang ditampilkan

unsigned long lastUpdate = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

String days[] = {"Minggu", "Senin", "Selasa", "Rabu", "Kamis", "Jumat", "Sabtu"};
String months[] = {"Jan", "Feb", "Mar", "Apr", "Mei", "Jun", "Jul", "Agu", "Sep", "Okt", "Nov", "Des"};
  
int currentDay = 0;   // 0 = Minggu, 1 = Senin, ..., 6 = Sabtu
int currentDate = 1;  // Tanggal (1-31)
int currentMonth = 0; // Bulan (0 = Jan, 11 = Des)

// Deklarasi pin untuk buzzer dan LED
const int buzzerPin = 17;
const int ledPin = 16;
unsigned long alertStartTime = 0; // Waktu saat peringatan dimulai
bool alertActive = false;         // Status apakah peringatan aktif
bool alertOn = false;             // Status apakah buzzer dan LED menyala
unsigned long lastBlinkTime = 0;  // Waktu terakhir berkedip
const unsigned long blinkInterval = 500; // Interval kedip (500ms)

//button
int displayMode = 1;
const int buttonPin = 0; // Pin untuk tombol
bool buttonPressed = false;

unsigned long lastButtonPress = 0;  // Waktu terakhir tombol ditekan
bool isScreenOn = true;            // Status layar (aktif atau mati)
const unsigned long SCREEN_TIMEOUT = 10000; // Timeout layar dalam milidetik (10 detik)

unsigned long lastValidBeatTime = 0;

void checkButton() {
  static bool lastButtonState = HIGH;
  bool currentButtonState = digitalRead(buttonPin);

  if (lastButtonState == HIGH && currentButtonState == LOW) { // Tombol ditekan
    lastButtonPress = millis(); // Catat waktu terakhir tombol ditekan

    if (!isScreenOn) {
      isScreenOn = true;       // Hidupkan layar jika mati
      tft.writecommand(0x29);  // Perintah untuk menghidupkan layar TFT
      Serial.println("Layar dihidupkan kembali.");
    } else {
      displayMode++;           // Ubah mode tampilan
      if (displayMode > 3) displayMode = 1; // Reset mode ke 1
    }

    delay(200); // Debounce
  }

  lastButtonState = currentButtonState;
}


// Fungsi untuk menyajikan halaman web
void handleHeartRate() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Heart Rate Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: Arial, sans-serif;
//      background: linear-gradient(to bottom, #fff6f6, #ffe6e6);
      color: #333;
      margin: 0;
      padding: 0;
    } 

    h1 {
      text-align: center;
      margin: 20px 10px;
      font-size: 28px;
      color: #007bff;
      text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.2);
    }

    canvas {
      display: block;
      margin: 20px auto;
      border-radius: 10px;
      border: 1px solid #ddd;
      background: #fff;
      box-shadow: 0px 6px 10px rgba(0, 0, 0, 0.1);
    }

    .info {
      text-align: center;
      font-size: 18px;
      margin: 10px 0;
    }

    .info span {
      font-weight: bold;
      color: #007bff;
    }

    .download-btn {
      display: block;
      margin: 20px auto;
      padding: 10px 20px;
      background-color: #007bff;
      color: white;
      border: none;
      font-size: 16px;
      cursor: pointer;
    }

    .download-btn:hover {
      background-color: #0056b3;
    }

    footer {
      text-align: center;
      padding: 10px 0;
      background: #007bff;
      color: white;
      font-size: 14px;
      margin-top: 20px;
    }

    .tooltip {
      position: absolute;
      display: none;
      background: #333;
      color: #fff;
      padding: 5px 10px;
      border-radius: 5px;
      font-size: 12px;
      pointer-events: none;
      box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
    }
  </style>
</head>
<body>
  <h1>Heart Rate Monitor</h1>

  <canvas id="heartRateChart" width="400" height="200"></canvas>
  <div class="info">Current Heart Rate: <span id="currentHR">--</span> BPM</div>

  <button class="download-btn" id="downloadBtn">Download Data as CSV</button>

  <div class="tooltip" id="tooltip"></div>

  <script>
    const heartRateCanvas = document.getElementById('heartRateChart');
    const heartRateCtx = heartRateCanvas.getContext('2d');
    const tooltip = document.getElementById('tooltip');
    const downloadBtn = document.getElementById('downloadBtn');

    const heartRateData = [];
    const labels = [];
    const maxDataPoints = 500;

    function drawChart(ctx, data, labels, color, maxY, title) {
      ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);

      // Draw title
      ctx.font = '16px Arial';
      ctx.fillStyle = '#007bff';
      ctx.fillText(title, 10, 20);

      // Draw axes
      ctx.strokeStyle = '#ccc';
      ctx.beginPath();
      ctx.moveTo(40, 180);
      ctx.lineTo(380, 180); // X-axis
      ctx.moveTo(40, 180);
      ctx.lineTo(40, 20);  // Y-axis
      ctx.stroke();

      // Plot data points
      ctx.strokeStyle = color;
      ctx.fillStyle = color;
      ctx.beginPath();
      const points = [];
      data.forEach((value, index) => {
        const x = 40 + (index * (340 / (maxDataPoints - 1)));
        const y = 180 - (value / maxY * 160);
        points.push({ x, y, value, label: labels[index] });
        if (index === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      });
      ctx.stroke();

      // Draw data points as circles
      points.forEach(({ x, y }) => {
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, 2 * Math.PI);
        ctx.fill();
      });

      return points;
    }

    function addHoverEffect(canvas, ctx, data, labels, color, maxY, title) {
      const points = drawChart(ctx, data, labels, color, maxY, title);

      canvas.addEventListener('mousemove', (e) => {
        const rect = canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        tooltip.style.display = 'none'; // Hide tooltip by default

        points.forEach(({ x, y, value, label }) => {
          const distance = Math.sqrt((x - mouseX) ** 2 + (y - mouseY) ** 2);
          if (distance < 5) {
            // Highlight point
            ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
            drawChart(ctx, data, labels, color, maxY, title);
            ctx.beginPath();
            ctx.arc(x, y, 5, 0, 2 * Math.PI);
            ctx.fillStyle = 'red';
            ctx.fill();

            // Show tooltip
            tooltip.style.display = 'block';
            tooltip.style.left = `${e.pageX + 10}px`;
            tooltip.style.top = `${e.pageY - 10}px`;
            tooltip.innerHTML = `Time: ${label}<br>Value: ${value} BPM`;
          }
        });
      });
    }

    async function fetchData() {
      try {
        const response = await fetch('/data');
        const { beatAvg } = await response.json();

        // Update data arrays
        const time = new Date().toLocaleTimeString();
        if (heartRateData.length >= maxDataPoints) {
          heartRateData.shift();
          labels.shift();
        }
        heartRateData.push(beatAvg);
        labels.push(time);

        // Update chart and hover effect
        addHoverEffect(heartRateCanvas, heartRateCtx, heartRateData, labels, '#007bff', 150, 'HeartRate (BPM)');

        // Update chart
        drawChart(heartRateCtx, heartRateData, labels, '#007bff', 150, 'Heart Rate (BPM)');

        // Update current value
        document.getElementById('currentHR').innerText = beatAvg || '--';
      } catch (error) {
        console.error("Error fetching data:", error);
      }
    }

    // Fetch data every second
    setInterval(fetchData, 1000);

   // Function to download data as CSV
    function downloadData() {
      let csvContent = "Time,Heart Rate (BPM)\n"; // Headers

      // Add data rows
      for (let i = 0; i < heartRateData.length; i++) {
        csvContent += `${labels[i]},${heartRateData[i]}\n`;
      }

      // Create a Blob from the CSV content
      const blob = new Blob([csvContent], { type: 'text/csv;charset=utf-8;' });
      
      // Create a download link
      const link = document.createElement('a');
      if (link.download !== undefined) {
        // Create a URL for the Blob and set the download attribute
        const url = URL.createObjectURL(blob);
        link.setAttribute('href', url);
        link.setAttribute('download', 'heart_rate_data.csv');
        
        // Simulate a click to download the file
        link.click();
      }
    }

    // Event listener for download button
    downloadBtn.addEventListener('click', downloadData);
    
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

// Halaman untuk grafik Temperature
void handleTemperature() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <title>Temperature Monitor</title>
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <style>
    body {
      font-family: Arial, sans-serif;
//      background: linear-gradient(to bottom, #fff6f6, #ffe6e6);
      color: #333;
      margin: 0;
      padding: 0;
    }

    h1 {
      text-align: center;
      margin: 20px 10px;
      font-size: 28px;
      color: #ff4500;
      text-shadow: 1px 1px 3px rgba(0, 0, 0, 0.2);
    }

    canvas {
      display: block;
      margin: 20px auto;
      border-radius: 10px;
      border: 1px solid #ddd;
      background: #fff;
      box-shadow: 0px 6px 10px rgba(0, 0, 0, 0.1);
    }

    .info {
      text-align: center;
      font-size: 18px;
      margin: 10px 0;
    }

    .info span {
      font-weight: bold;
      color: #ff4500;
    }

    footer {
      text-align: center;
      padding: 10px 0;
      background: #ff4500;
      color: white;
      font-size: 14px;
      margin-top: 20px;
    }

    .tooltip {
      position: absolute;
      display: none;
      background: #333;
      color: #fff;
      padding: 5px 10px;
      border-radius: 5px;
      font-size: 12px;
      pointer-events: none;
      box-shadow: 0px 4px 6px rgba(0, 0, 0, 0.1);
    }
  </style>
</head>
<body>
  <h1>Temperature Monitor</h1>

  <canvas id="temperatureChart" width="400" height="200"></canvas>
  <div class="info">Current Temperature: <span id="currentTemp">--</span>C</div>
//  <footer>2024 Temperature Monitoring System</footer>

  <div class="tooltip" id="tooltip"></div>

  <script>
    const tempCanvas = document.getElementById('temperatureChart');
    const tempCtx = tempCanvas.getContext('2d');
    const tooltip = document.getElementById('tooltip');

    const tempData = [];
    const labels = [];
    const maxDataPoints = 50;

    function drawChart(ctx, data, labels, color, maxY, title) {
      ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);

      // Draw title
      ctx.font = '16px Arial';
      ctx.fillStyle = '#ff4500';
      ctx.fillText(title, 10, 20);

      // Draw axes
      ctx.strokeStyle = '#ccc';
      ctx.beginPath();
      ctx.moveTo(40, 180);
      ctx.lineTo(380, 180); // X-axis
      ctx.moveTo(40, 180);
      ctx.lineTo(40, 20);  // Y-axis
      ctx.stroke();

      // Plot data points
      ctx.strokeStyle = color;
      ctx.fillStyle = color;
      ctx.beginPath();
      const points = [];
      data.forEach((value, index) => {
        const x = 40 + (index * (340 / (maxDataPoints - 1)));
        const y = 180 - (value / maxY * 160);
        points.push({ x, y, value, label: labels[index] });
        if (index === 0) {
          ctx.moveTo(x, y);
        } else {
          ctx.lineTo(x, y);
        }
      });
      ctx.stroke();

      // Draw data points as circles
      points.forEach(({ x, y }) => {
        ctx.beginPath();
        ctx.arc(x, y, 3, 0, 2 * Math.PI);
        ctx.fill();
      });

      return points;
    }

    function addHoverEffect(canvas, ctx, data, labels, color, maxY, title) {
      const points = drawChart(ctx, data, labels, color, maxY, title);

      canvas.addEventListener('mousemove', (e) => {
        const rect = canvas.getBoundingClientRect();
        const mouseX = e.clientX - rect.left;
        const mouseY = e.clientY - rect.top;

        tooltip.style.display = 'none'; // Hide tooltip by default

        points.forEach(({ x, y, value, label }) => {
          const distance = Math.sqrt((x - mouseX) ** 2 + (y - mouseY) ** 2);
          if (distance < 5) {
            // Highlight point
            ctx.clearRect(0, 0, ctx.canvas.width, ctx.canvas.height);
            drawChart(ctx, data, labels, color, maxY, title);
            ctx.beginPath();
            ctx.arc(x, y, 5, 0, 2 * Math.PI);
            ctx.fillStyle = 'red';
            ctx.fill();

            // Show tooltip
            tooltip.style.display = 'block';
            tooltip.style.left = `${e.pageX + 10}px`;
            tooltip.style.top = `${e.pageY - 10}px`;
            tooltip.innerHTML = `Time: ${label}<br>Value: ${value} °C`;
          }
        });
      });
    }

    async function fetchData() {
      try {
        const response = await fetch('/data');
        const { temperature } = await response.json();

        // Update data arrays
        const time = new Date().toLocaleTimeString();
        if (tempData.length >= maxDataPoints) {
          tempData.shift();
          labels.shift();
        }
        tempData.push(temperature);
        labels.push(time);

        // Update chart and hover effect
        addHoverEffect(tempCanvas, tempCtx, tempData, labels, '#ff4500', 50, 'Temperature (°C)');

        // Update current value
        document.getElementById('currentTemp').innerText = temperature || '--';
      } catch (error) {
        console.error("Error fetching data:", error);
      }
    }

    // Fetch data every second
    setInterval(fetchData, 1000);
  </script>
</body>
</html>
)rawliteral";

  server.send(200, "text/html", html);
}

void handlesetup(){
  String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
      <title>Setup Clock and Heart Rate Limit</title>
      <style>
        body { font-family: Arial, sans-serif; text-align: center; margin: 20px; }
        form { display: inline-block; text-align: left; margin: 20px; padding: 20px; border: 1px solid #ddd; border-radius: 5px; }
        label, input, select { display: block; margin-bottom: 10px; }
        input[type="number"] { width: 100%; padding: 5px; }
        button { padding: 10px 20px; background-color: #4CAF50; color: white; border: none; cursor: pointer; }
      </style>
    </head>
    <body>
      <h1>Setup Clock and Heart Rate Limit</h1>
      
      <!-- Formulir untuk mengatur waktu -->
      <form action="/set_time" method="POST">
        <h2>Set Time and Date</h2>
        <label for="hours">Hours (0-23):</label>
        <input type="number" id="hours" name="hours" min="0" max="23" required>
        <label for="minutes">Minutes (0-59):</label>
        <input type="number" id="minutes" name="minutes" min="0" max="59" required>
        <label for="seconds">Seconds (0-59):</label>
        <input type="number" id="seconds" name="seconds" min="0" max="59" required>
        <label for="day">Day:</label>
        <select id="day" name="day">
          <option value="0">Minggu</option>
          <option value="1">Senin</option>
          <option value="2">Selasa</option>
          <option value="3">Rabu</option>
          <option value="4">Kamis</option>
          <option value="5">Jumat</option>
          <option value="6">Sabtu</option>
        </select>
        <label for="date">Date (1-31):</label>
        <input type="number" id="date" name="date" min="1" max="31" required>
        <label for="month">Month:</label>
        <select id="month" name="month">
          <option value="0">Jan</option>
          <option value="1">Feb</option>
          <option value="2">Mar</option>
          <option value="3">Apr</option>
          <option value="4">Mei</option>
          <option value="5">Jun</option>
          <option value="6">Jul</option>
          <option value="7">Agu</option>
          <option value="8">Sep</option>
          <option value="9">Okt</option>
          <option value="10">Nov</option>
          <option value="11">Des</option>
        </select>
        <button type="submit">Set Time and Date</button>
      </form>

      <!-- Formulir untuk mengatur batas denyut jantung -->
      <form action="/set_limit" method="POST">
        <h2>Set Heart Rate Limit</h2>
        <label for="limitHeartRate">Heart Rate Limit:</label>
        <input type="number" id="limitHeartRate" name="limitHeartRate" min="30" max="200" value="50" required>
        <button type="submit">Set Heart Rate Limit</button>
      </form>
    </body>
    </html>
  )rawliteral";
  server.send(200, "text/html", html);
  }

void handleSetTime() {
  if (server.hasArg("hours") && server.hasArg("minutes") && server.hasArg("seconds") &&
      server.hasArg("day") && server.hasArg("date") && server.hasArg("month")) {
    hours = server.arg("hours").toInt();
    minutes = server.arg("minutes").toInt();
    seconds = server.arg("seconds").toInt();
    currentDay = server.arg("day").toInt();
    currentDate = server.arg("date").toInt();
    currentMonth = server.arg("month").toInt();

    Serial.printf("Time updated: %02d:%02d:%02d\n", hours, minutes, seconds);
    Serial.printf("Date updated: %s, %02d %s\n",
                  days[currentDay].c_str(), currentDate, months[currentMonth].c_str());

    server.send(200, "text/plain", "Time and date updated successfully!");
  } else {
    server.send(400, "text/plain", "Invalid input!");
  }
}

void handleSetLimit() {
  if (server.hasArg("limitHeartRate")) {
    limitHeartRate = server.arg("limitHeartRate").toInt();

    Serial.printf("Heart rate limit updated: %d\n", limitHeartRate);
    server.send(200, "text/plain", "Heart rate limit updated successfully!");
  } else {
    server.send(400, "text/plain", "Invalid input!");
  }
}



// Fungsi untuk memberikan data JSON detak jantung
void handleData() {
  String json = "{\"beatAvg\": " + String(beatAvg) + ", \"temperature\": " + String(temperature) + "}";
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", json);
}

void setup() {
  // Inisialisasi serial untuk debugging
  Serial.begin(115200);

  Serial.println("Initializing...");

  // Inisialisasi Wi-Fi sebagai Access Point
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("Wi-Fi Access Point Started!");
  Serial.print("Access Point IP Address: ");
  Serial.println(WiFi.softAPIP());

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

  // Atur handler untuk web server
  server.on("/", handleHeartRate);
  server.on("/device", handleTemperature);
  server.on("/data", handleData); // Endpoint JSON
  server.on("/setup", HTTP_GET, handlesetup);
  server.on("/set_time", HTTP_POST, handleSetTime);
  server.on("/set_limit", HTTP_POST, handleSetLimit);
  server.begin();
  Serial.println("Web server started.");
}

void loop() {
  server.handleClient();
  
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

      lastValidBeatTime = millis();
    }
    else{
      if (millis() - lastValidBeatTime >= 10000) {
        beatAvg = 70;
    }
      }
  }
//  else beatAvg = 0;

  // Periksa apakah beatAvg melebihi 80 dan nyalakan peringatan jika demikian
  if (beatAvg > limitHeartRate && !alertActive) {
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

     // Tambahkan nilai beatAvg ke buffer
    beatAvgBuffer[bufferIndex++] = beatAvg;
    bufferIndex %= AVG_BUFFER_SIZE;  // Bungkus indeks agar tetap dalam rentang

    // Hitung rata-rata dari buffer
    float sum = 0;
    for (int i = 0; i < AVG_BUFFER_SIZE; i++) {
        sum += beatAvgBuffer[i];
    }
    displayedBeatAvg = sum / AVG_BUFFER_SIZE;

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

    if (displayMode == 1) {
    tft.fillScreen(TFT_BLACK);
    
    // Warna latar belakang dan elemen
    uint16_t bgColor = TFT_BLACK;
    uint16_t textColor = TFT_WHITE;
    uint16_t accentColor = TFT_CYAN;

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
    
    if (displayMode == 2) {
    tft.fillScreen(TFT_BLACK);

    // Warna elemen
    uint16_t bgColor = TFT_BLACK;
    uint16_t textColor = TFT_WHITE;
    uint16_t accentColor = TFT_CYAN;
    uint16_t heartColor = (beatAvg > 100) ? TFT_RED : TFT_GREEN;

    // Header: "Heart Info"
    String header = "Heart Info";
    tft.setTextSize(3);
    int headerWidth = header.length() * 18; // 18 adalah lebar karakter rata-rata untuk setTextSize(3)
    tft.setTextColor(accentColor, bgColor);
    tft.setCursor((240 - headerWidth) / 2, 10); // Posisikan di tengah
    tft.print(header);

    // Garis dekoratif
    tft.drawLine(10, 40, 230, 40, accentColor);

    // Gambar hati dengan animasi dinamis (ukuran berubah dengan BPM)
    int heartSize = 40 + (beatAvg > 100 ? 5 : 0);
    tft.fillCircle(100, 90, heartSize, heartColor); // Hati kiri atas
    tft.fillCircle(140, 90, heartSize, heartColor); // Hati kanan atas
    tft.fillTriangle(65, 90, 180, 80, 120, 160 + (heartSize - 40), heartColor); // Bagian bawah hati

    // Tampilkan BPM besar
    String bpmText = String(displayedBeatAvg) + " BPM";
    tft.setTextSize(4);
    int bpmWidth = bpmText.length() * 24; // 24 adalah lebar karakter rata-rata untuk setTextSize(4)
    tft.setCursor((240 - bpmWidth) / 2, 180); // Posisikan di tengah
    tft.setTextColor(textColor, bgColor);
    tft.print(bpmText);

    // Indikator level detak jantung
    String status = (displayedBeatAvg > 100) ? "High" : (displayedBeatAvg < 60) ? "Low" : "Normal";
    uint16_t statusColor = (displayedBeatAvg > 100) ? TFT_RED : (displayedBeatAvg < 60) ? TFT_YELLOW : TFT_GREEN;
    String statusText = "Status: " + status;
    tft.setTextSize(2);
    int statusWidth = statusText.length() * 12; // 12 adalah lebar karakter rata-rata untuk setTextSize(2)
    tft.setCursor((240 - statusWidth) / 2, 230); // Posisikan di tengah
    tft.setTextColor(statusColor, bgColor);
    tft.print(statusText);

    // Catatan kecil
    String note = "Normal: 60-100 BPM.";
    tft.setTextSize(1);
    int noteWidth = note.length() * 6; // 6 adalah lebar karakter rata-rata untuk setTextSize(1)
    tft.setCursor((240 - noteWidth) / 2, 260); // Posisikan di tengah
    tft.setTextColor(TFT_YELLOW, bgColor);
    tft.print(note);
}

    else if (displayMode == 3) {
    // Hapus tampilan sebelumnya
    tft.fillScreen(TFT_BLACK);

    // Warna elemen
    uint16_t bgColor = TFT_BLACK;
    uint16_t textColor = TFT_WHITE;
    uint16_t accentColor = TFT_CYAN;

    // Header: "Wi-Fi Info"
    tft.setTextSize(3);
    tft.setTextColor(accentColor, bgColor);
    tft.setCursor(30, 10);
    tft.print("Wi-Fi Info");

    // Garis pemisah
    tft.drawLine(10, 40, 230, 40, accentColor);

    // Ikon Wi-Fi
    tft.fillCircle(30, 70, 10, accentColor); // Titik utama
    tft.fillTriangle(20, 70, 40, 70, 30, 90, accentColor); // Panah bawah
    tft.drawCircle(30, 70, 14, accentColor); // Lingkaran luar
    tft.drawCircle(30, 70, 20, accentColor); // Lingkaran luar kedua

    // Tampilkan IP Address
    tft.setTextSize(2);
    tft.setTextColor(textColor, bgColor);
    tft.setCursor(60, 65);
    tft.print("IP:");
    tft.setCursor(100, 65);
    tft.print(WiFi.softAPIP());

    // Tampilkan SSID (Nama Wi-Fi)
    tft.setCursor(20, 110);
    tft.setTextColor(TFT_GREEN, bgColor);
    tft.print("SSID: ");
    tft.setCursor(100, 110);
    tft.setTextColor(textColor, bgColor);
    tft.print(ap_ssid);

    // Tampilkan Password
    tft.setCursor(20, 140);
    tft.setTextColor(TFT_RED, bgColor);
    tft.print("PASS:");
    tft.setCursor(100, 140);
    tft.setTextColor(textColor, bgColor);
    tft.print(ap_password);

    // Garis bawah dekoratif 
    tft.drawLine(10, 180, 230, 180, accentColor);

    // Catatan atau petunjuk
    tft.setTextSize(1);
    tft.setTextColor(TFT_YELLOW, bgColor);
    tft.setCursor(45, 200);
    tft.print("Connect to Wi-Fi using the");
    tft.setCursor(70, 215);
    tft.print("above credentials.");
    }
  }
  
  checkButton();
   // Periksa apakah layar perlu dimatikan
  if (isScreenOn && millis() - lastButtonPress > SCREEN_TIMEOUT) {
    isScreenOn = false;       // Matikan layar
    tft.writecommand(0x28);   // Perintah untuk mematikan layar TFT
    Serial.println("Layar dimatikan karena tidak ada aktivitas.");
  }
}
