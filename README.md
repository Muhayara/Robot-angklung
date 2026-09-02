# Robot Angklung ESP32 (IoT)

Proyek ini adalah sistem kontrol otomatis untuk robot Angklung berbasis **ESP32**, menggunakan komunikasi MQTT dan antarmuka web (Captive Portal/Web Server) untuk konfigurasi jaringan WiFi.

## 🛠️ Fitur Utama
- **Kontrol Motor via MQTT**: ESP32 mendengarkan topik MQTT untuk menggerakkan relay/motor DC (JGA25-370).
- **WiFi Manager Bawaan**: Jika gagal terhubung ke WiFi, alat akan memancarkan Access Point (`Angklung-Robot`) untuk setup jaringan lewat Web UI.
- **Indikator RGB**: LED RGB untuk menampilkan status perangkat:
  - 🔴 Merah = Booting
  - 🟢 Hijau = WiFi Terhubung
  - 🔵 Biru = MQTT Terhubung & Siap menerima perintah

## ⚙️ Perangkat Keras (Hardware)
- **Microcontroller**: ESP32 (ESP-WROOM-32 / Dev Module)
- **Aktuator**: Motor DC JGA25-370 (dikontrol via Relay)
- **Komunikasi**: Modul WiFi bawaan ESP32

### Konfigurasi Pin (Wiring)
| Komponen | Pin ESP32 | Catatan |
|---|---|---|
| Relay CH 0 | GPIO 13 | Active LOW (Motor JGA25-370) |
| Relay CH 1 | GPIO 14 | - |
| Relay CH 2 | GPIO 18 | - |
| Relay CH 3 | GPIO 19 | - |
| Relay CH 4 | GPIO 21 | - |
| Relay CH 5 | GPIO 22 | - |
| Relay CH 6 | GPIO 23 | - |
| Relay CH 7 | GPIO 25 | - |
| LED RGB (R) | GPIO 26 | Indikator Booting |
| LED RGB (G) | GPIO 27 | Indikator WiFi |
| LED RGB (B) | GPIO 32 | Indikator MQTT |

## 🚀 Cara Menjalankan (Development)
Proyek ini dibangun menggunakan **PlatformIO**. 

1. Pastikan Anda sudah menginstal [VSCode](https://code.visualstudio.com/) dan ekstensi **PlatformIO**.
2. *Clone* repositori ini:
   ```bash
   git clone https://github.com/USERNAME_ANDA/angklung-esp32.git
   ```
3. Buka folder proyek di VSCode. PlatformIO akan otomatis mengunduh library yang dibutuhkan (didefinisikan di `platformio.ini`).
4. Klik tombol **Build** (Tanda centang) dan **Upload** (Tanda panah ke kanan) di bagian bawah editor.

## 🔒 Catatan Keamanan
Kredensial bawaan (WiFi & Topik MQTT) di dalam `src/main.cpp` adalah data *dummy* untuk tujuan demonstrasi. 
Jika Anda akan menggunakan ini di lingkungan *production*, pastikan Anda mengubah:
- `AP_PASSWORD`
- `MQTT_TOPIC`
ke versi yang aman.

## 📄 Lisensi & HAKI
**Copyright (c) 2026 - All Rights Reserved.**

Proyek ini sedang dalam tahap pendaftaran **Hak Kekayaan Intelektual (HAKI)**. Kode sumber ini diunggah untuk keperluan administratif dan portofolio. Dilarang keras menyalin, mendistribusikan, memodifikasi, atau menggunakan kode ini untuk kepentingan komersial tanpa izin tertulis dari pencipta.
