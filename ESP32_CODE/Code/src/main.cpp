#include <WiFi.h>
#include <WebServer.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>

// Konfigurasi Wi-Fi
const char* ssid = "Esp32_surya";
const char* password = "surya123";

// Inisialisasi Hardware
LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
Servo servoPintu;

// Pin Hardware
const int PIN_RELAY_MESIN = 18; // Relay 1 (Mesin)
const int PIN_RELAY_PINTU = 19; // Relay 2 (Pintu/Power Servo)
const int PIN_SERVO       = 17; // Pin Data Servo Motor
const int PIN_TOMBOL      = 23; // Tombol Fisik Buka Pintu

// Variabel Status & Timer
bool prosesBerjalan = false;
bool pintuBisaDibuka = false;

unsigned long waktuMulai = 0;
const unsigned long JEDA_PERINGATAN = 5000;   // 5 detik
const unsigned long DUKUNGAN_MESIN = 20000;  // 20 detik
const unsigned long TOTAL_WAKTU = JEDA_PERINGATAN + DUKUNGAN_MESIN; // 25 detik total

// Catatan Logika Relay (Aktif LOW umumnya pada modul relay ESP32):
// LOW  = Relay MENYALA (ON)
// HIGH = Relay MATI (OFF)
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// Fungsi Animasi Teks Berjalan
void animasiTeks(String teks, int baris, int kecepatan) {
  String pesan = "                " + teks + "                ";
  for (int i = 0; i < pesan.length() - 16; i++) {
    lcd.setCursor(0, baris);
    lcd.print(pesan.substring(i, i + 16));
    delay(kecepatan);
  }
}

// Halaman Web UI Modern
void tanganiRoot() {
  String html = "<!DOCTYPE html><html lang='id'><head>";
  html += "<meta charset='UTF-8'><meta name='viewport' content='width=device-width, initial-scale=1.0'>";
  html += "<title>Kontrol Projek Plastika</title>";
  html += "<style>";
  html += "  * { box-sizing: border-box; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }";
  html += "  body { background: linear-gradient(135deg, #1e3c72 0%, #2a5298 100%); margin: 0; padding: 20px; color: #333; min-height: 100vh; display: flex; align-items: center; justify-content: center; }";
  html += "  .card { background: #ffffff; padding: 30px; border-radius: 20px; box-shadow: 0 15px 30px rgba(0,0,0,0.3); max-width: 450px; width: 100%; text-align: center; }";
  html += "  h1 { color: #1e3c72; margin-bottom: 5px; font-size: 24px; text-transform: uppercase; letter-spacing: 1px; }";
  html += "  .subtitle { color: #777; font-size: 14px; margin-bottom: 25px; }";
  html += "  .status-badge { display: inline-block; padding: 10px 20px; border-radius: 50px; background: #e0e0e0; font-weight: bold; font-size: 14px; color: #444; margin-bottom: 25px; transition: all 0.3s; }";
  html += "  .btn { display: block; width: 100%; padding: 16px; font-size: 16px; font-weight: bold; border: none; border-radius: 12px; cursor: pointer; transition: all 0.3s ease; text-decoration: none; margin-bottom: 15px; box-shadow: 0 4px 10px rgba(0,0,0,0.15); }";
  html += "  .btn-start { background: linear-gradient(45deg, #11998e, #38ef7d); color: white; }";
  html += "  .btn-start:hover { transform: translateY(-2px); box-shadow: 0 6px 15px rgba(56,239,125,0.4); }";
  html += "  .btn-door { background: linear-gradient(45deg, #FF512F, #DD2476); color: white; }";
  html += "  .btn-door:hover { transform: translateY(-2px); box-shadow: 0 6px 15px rgba(221,36,118,0.4); }";
  html += "  .progress-container { background-color: #e9ecef; border-radius: 15px; height: 25px; width: 100%; margin: 20px 0; overflow: hidden; box-shadow: inset 0 2px 5px rgba(0,0,0,0.1); }";
  html += "  .progress-bar { height: 100%; width: 0%; background: linear-gradient(90deg, #ff9900, #ff5500); text-align: center; line-height: 25px; color: white; font-size: 12px; font-weight: bold; transition: width 0.4s ease; }";
  html += "</style>";
  html += "<script>";
  html += "setInterval(function() {";
  html += "  fetch('/status').then(r => r.json()).then(data => {";
  html += "    document.getElementById('bar').style.width = data.progress + '%';";
  html += "    document.getElementById('bar').innerText = data.progress + '%';";
  html += "    document.getElementById('statusText').innerText = data.status;";
  html += "    if(data.pintu) { document.getElementById('btnDoor').style.display = 'block'; }";
  html += "    else { document.getElementById('btnDoor').style.display = 'none'; }";
  html += "  });";
  html += "}, 500);";
  html += "</script></head><body>";
  
  html += "<div class='card'>";
  html += "  <h1>Projek Plastika</h1>";
  html += "  <div class='subtitle'>Panel Kontrol Mesin Otomatis</div>";
  html += "  <div class='status-badge' id='statusText'>Sistem Siap</div>";
  
  html += "  <a href='/start'><button class='btn btn-start'>⚡ AKTIFKAN MESIN</button></a>";
  html += "  <a href='/open-door'><button id='btnDoor' class='btn btn-door' style='display:none;'>🔓 BUKA PINTU MESIN</button></a>";
  
  html += "  <div class='progress-container'><div id='bar' class='progress-bar'>0%</div></div>";
  html += "</div>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

// Endpoint status AJAX
void tanganiStatus() {
  int progress = 0;
  String status = "STANDBY";

  if (prosesBerjalan) {
    unsigned long berlalu = millis() - waktuMulai;
    if (berlalu < JEDA_PERINGATAN) {
      status = "⚠️ PERINGATAN: JAUHKAN TANGAN!";
      progress = (berlalu * 100) / TOTAL_WAKTU;
    } else if (berlalu < TOTAL_WAKTU) {
      status = "⚙️ MESIN SEDANG AKTIF";
      progress = (berlalu * 100) / TOTAL_WAKTU;
    }
  } else if (pintuBisaDibuka) {
    status = "✅ PROSES SELESAI - SILAKAN BUKA PINTU";
    progress = 100;
  }

  String json = "{\"progress\":" + String(progress) + ", \"status\":\"" + status + "\", \"pintu\":" + String(pintuBisaDibuka ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// Trigger Mulai Mesin
void tanganiStart() {
  if (!prosesBerjalan && !pintuBisaDibuka) {
    prosesBerjalan = true;
    pintuBisaDibuka = false;
    waktuMulai = millis();
    digitalWrite(PIN_RELAY_MESIN, RELAY_ON); // Relay Mesin Aktif
  }
  server.sendHeader("Location", "/");
  server.send(303);
}

// Fungsi Buka Pintu dengan Relay + Servo
void BukaPintuAction() {
  if (pintuBisaDibuka) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMBUKA PINTU...");

    // 1. Aktifkan Relay Pintu
    digitalWrite(PIN_RELAY_PINTU, RELAY_ON);
    delay(200);

    // 2. Servo Putar ke 180 Derajat
    servoPintu.write(180);
    lcd.setCursor(0, 1);
    lcd.print("PINTU TERBUKA   ");
    delay(5000); // Tahan selama 5 detik

    // 3. Servo Putar Balik ke 0 Derajat (Tutup)
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MENUTUP PINTU...");
    servoPintu.write(0);
    delay(1000); 

    // 4. Matikan Relay Pintu
    digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);

    pintuBisaDibuka = false;

    // Reset Tampilan ke IP Address
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
  }
}

// Trigger Buka Pintu dari Web
void tanganiBukaPintu() {
  BukaPintuAction();
  server.sendHeader("Location", "/");
  server.send(303);
}

void setup() {
  // Inisialisasi Relay dalam posisi MATI TERLEBIH DAHULU
  digitalWrite(PIN_RELAY_MESIN, RELAY_OFF);
  digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);
  
  pinMode(PIN_RELAY_MESIN, OUTPUT);
  pinMode(PIN_RELAY_PINTU, OUTPUT);
  pinMode(PIN_TOMBOL, INPUT_PULLUP);

  // Inisialisasi Servo
  servoPintu.attach(PIN_SERVO);
  servoPintu.write(0); // Posisi awal 0 derajat (Terkunci)

  // Inisialisasi LCD
  lcd.init();
  lcd.backlight();

  // 1. Tampilan Booting & Animasi Teks
  lcd.setCursor(0, 0);
  lcd.print("PROJEK PLASTIKA");
  animasiTeks("SELAMAT DATANG DI PROJEK PLASTIKA", 1, 150);

  // 2. Koneksi Wi-Fi
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Connecting WiFi");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    lcd.print(".");
  }

  // 3. Tampilkan IP Address
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("IP Address:");
  lcd.setCursor(0, 1);
  lcd.print(WiFi.localIP());

  // Routing Web Server
  server.on("/", tanganiRoot);
  server.on("/status", tanganiStatus);
  server.on("/start", tanganiStart);
  server.on("/open-door", tanganiBukaPintu);
  server.begin();
}

void loop() {
  server.handleClient();

  // Membaca Tombol Fisik Pintu
  if (digitalRead(PIN_TOMBOL) == LOW && pintuBisaDibuka) {
    BukaPintuAction();
  }

  // Logika Siklus Kerja Mesin
  if (prosesBerjalan) {
    unsigned long waktuSekarang = millis() - waktuMulai;

    // Fase 1: Peringatan 5 Detik Pertama
    if (waktuSekarang < JEDA_PERINGATAN) {
      lcd.setCursor(0, 0);
      lcd.print("-- PERINGATAN --");
      lcd.setCursor(0, 1);
      lcd.print("JAUHKAN TANGAN! ");
    } 
    // Fase 2: Hitung Mundur Mesin Aktif (20 Detik)
    else if (waktuSekarang < TOTAL_WAKTU) {
      int sisaWaktu = (TOTAL_WAKTU - waktuSekarang) / 1000;
      lcd.setCursor(0, 0);
      lcd.print("  MESIN AKTIF   ");
      lcd.setCursor(0, 1);
      lcd.print("Timer: " + String(sisaWaktu) + " Detik   ");
    } 
    // Fase 3: Selesai
    else {
      digitalWrite(PIN_RELAY_MESIN, RELAY_OFF); // Matikan Relay Mesin
      prosesBerjalan = false;
      pintuBisaDibuka = true;

      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("PROSES SELESAI");
      lcd.setCursor(0, 1);
      lcd.print("SILAKAN BUKA");
    }
  }
}