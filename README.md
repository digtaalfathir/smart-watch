#  ESP32 Web-based Heart Rate & Temperature Monitor

Alat ini adalah sistem pemantauan detak jantung dan suhu tubuh secara real-time menggunakan **ESP32**, **sensor MAX30105**, dan **layar TFT**, yang dilengkapi dengan antarmuka **web** untuk visualisasi data, pengaturan sistem, dan penyimpanan hasil pengukuran. Proyek ini dirancang sebagai solusi monitoring kesehatan berbasis IoT dengan tampilan dan fitur lengkap.

## ✨ Fitur Utama

- 🔴 **Monitoring detak jantung** menggunakan sensor MAX30105.
- 🌡️ **Pengukuran suhu tubuh** berbasis data sensor.
- 🧠 **Analisis Heart Rate** dan notifikasi ketika melebihi ambang batas.
- 📟 **Tampilan visual TFT**: jam digital, suhu, dan grafik detak jantung.
- 🌐 **Web server ESP32**:
  - `/` → Tampilan grafik Heart Rate.
  - `/device` → Tampilan grafik Suhu.
  - `/setup` → Pengaturan waktu dan batas detak.
  - `/data` → JSON data untuk API/web chart.
  - `/download` → CSV log data.
- 🔊 **Alarm otomatis** dengan buzzer & LED jika Heart Rate melewati batas.
- 📁 **Pencatatan data** dalam format `.csv`.
- 🔧 **Form Setup** untuk atur waktu & ambang batas HR melalui web.

## 🧩 Hardware yang Dibutuhkan

| Komponen           | Keterangan                    |
|--------------------|-------------------------------|
| ESP32 Dev Board    | Modul utama                   |
| MAX30105 Sensor    | Sensor detak jantung & oksigen|
| TFT LCD (ST7735)   | Layar visualisasi data        |
| Buzzer aktif       | Alarm ketika HR melebihi batas|
| LED                | Indikator status              |
| Breadboard & Kabel | Koneksi                       |

## 📡 Instalasi dan Penggunaan

### 1. Persiapan Arduino IDE
- Install **ESP32 Board** dari Board Manager.
- Install library berikut:
  - `WiFi.h`
  - `WebServer.h`
  - `TFT_eSPI` (konfigurasi sesuai display)
  - `Adafruit_GFX`
  - `MAX30105`
  - `heartRate.h` (tersedia dalam library MAX3010x)
  - `Wire.h`
  - `FS.h`, `SPIFFS.h`

### 2. Konfigurasi `TFT_eSPI`
Edit `User_Setup.h` atau gunakan `User_Setups/SetupXX.h` yang sesuai dengan display milikmu.

### 3. Upload Program
- Pastikan board: **ESP32 Dev Module**.
- Upload sketch via Arduino IDE.
- Buka Serial Monitor untuk melihat info IP address.

### 4. Koneksi Web
- ESP32 membuat **Access Point** bernama `BayMax` dengan password `12345678`.
- Hubungkan HP/PC ke Wi-Fi tersebut.
- Buka browser ke alamat: `http://192.168.4.1/`

### 5. Navigasi Halaman
- `/` → Grafik Heart Rate
- `/device` → Grafik Suhu Tubuh
- `/setup` → Form pengaturan jam dan batas detak
- `/download` → Download data log CSV

## 📁 Struktur Proyek

```
BayMax/
├── BayMax.ino              # File utama sketch Arduino
├── data/
│   └── data.csv            # File log hasil pengukuran
├── README.md               # Dokumentasi proyek
```

## ⚙️ Konfigurasi Default

| Parameter            | Nilai Default |
|----------------------|---------------|
| SSID AP              | `BayMax`      |
| Password AP          | `12345678`    |
| Batas Detak Jantung  | `100 bpm`     |
| Format CSV Log       | `Jam, Menit, Detik, Suhu, Detak` |

## 🔐 Catatan Keamanan

- Gantilah password default AP (`12345678`) pada kode sumber sebelum digunakan dalam lingkungan terbuka.
- Hindari penyimpanan data sensitif pada SPIFFS tanpa enkripsi jika digunakan untuk aplikasi nyata.

## ✅ Status

- [x] Monitoring detak jantung & suhu
- [x] Visualisasi di layar & web
- [x] Pengaturan sistem melalui web
- [x] Penyimpanan data `.csv`
- [ ] Pengiriman data ke cloud (opsional - belum implementasi)

## 🧑‍💻 Kontributor

Proyek dikembangkan oleh [Nama Kamu] sebagai bagian dari pengembangan sistem pemantauan kesehatan berbasis IoT menggunakan ESP32.
