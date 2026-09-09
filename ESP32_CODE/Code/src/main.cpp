#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <LittleFS.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <ESP32Servo.h>
#include <LittleFS.h>


// ============================================================
// KONFIGURASI WIFI & PERANGKAT
// ============================================================
const char* SSID_NAME = "Nanda";
const char* PASS_WORD = "NO RISK NO FERRARI";

LiquidCrystal_I2C lcd(0x27, 16, 2);
WebServer server(80);
Servo servoPintu;

// ============================================================
// PIN HARDWARE
// ============================================================
const int PIN_RELAY_MESIN = 18;
const int PIN_RELAY_PINTU = 19;
const int PIN_SERVO       = 17;
const int PIN_TOMBOL      = 23;

// Active LOW Relay Definitions
#define RELAY_ON  LOW
#define RELAY_OFF HIGH

// ============================================================
// STATUS SISTEM & TIMER
// ============================================================
bool prosesBerjalan  = false;
bool pintuBisaDibuka = false;

unsigned long waktuMulai = 0;
const unsigned long JEDA_PERINGATAN = 5000;   // 5 detik peringatan
const unsigned long DUKUNGAN_MESIN  = 20000;  // 20 detik mesin aktif
const unsigned long TOTAL_WAKTU     = JEDA_PERINGATAN + DUKUNGAN_MESIN; // 25 detik

// ============================================================
// HELPER & TAMPILAN LCD
// ============================================================
void animasiTeks(String teks, int baris, int kecepatan) {
    String pesan = "                " + teks + "                ";
    for (int i = 0; i < pesan.length() - 16; i++) {
        lcd.setCursor(0, baris);
        lcd.print(pesan.substring(i, i + 16));
        delay(kecepatan);
    }
}

void tampilkanIP() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("IP Address:");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.localIP());
}

// ============================================================
// ACTION HANDLERS
// ============================================================
void bukaPintuAction() {
    if (!pintuBisaDibuka) return;

    // Buka pintu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MEMBUKA PINTU...");

    digitalWrite(PIN_RELAY_PINTU, RELAY_ON);
    delay(200);
    servoPintu.write(180);

    lcd.setCursor(0, 1);
    lcd.print("PINTU TERBUKA");
    delay(5000); // Pintu terbuka selama 5 detik

    // Tutup pintu
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("MENUTUP PINTU...");

    servoPintu.write(0);
    delay(1000);

    digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);
    pintuBisaDibuka = false;

    tampilkanIP();
}

// ============================================================
// WEB SERVER HANDLERS
// ============================================================
void tanganiRoot() {
    File file = LittleFS.open("/index.html", "r");
    if (!file) {
        server.send(404, "text/plain", "index.html tidak ditemukan");
        return;
    }
    server.streamFile(file, "text/html");
    file.close();
}

void tanganiCSS() {
    File file = LittleFS.open("/style.css", "r");
    if (!file) {
        server.send(404, "text/plain", "style.css tidak ditemukan");
        return;
    }
    server.streamFile(file, "text/css");
    file.close();
}

void tanganiJS() {
    File file = LittleFS.open("/script.js", "r");
    if (!file) {
        server.send(404, "text/plain", "script.js tidak ditemukan");
        return;
    }
    server.streamFile(file, "application/javascript");
    file.close();
}

void tanganiStatus() {
    int progress = 0;
    String status = "STANDBY";

    if (prosesBerjalan) {
        unsigned long waktuBerlalu = millis() - waktuMulai;

        if (waktuBerlalu < JEDA_PERINGATAN) {
            status = "PERINGATAN: JAUHKAN TANGAN!";
            progress = (waktuBerlalu * 100) / TOTAL_WAKTU;
        } else if (waktuBerlalu < TOTAL_WAKTU) {
            status = "MESIN SEDANG AKTIF";
            progress = (waktuBerlalu * 100) / TOTAL_WAKTU;
        }
    } else if (pintuBisaDibuka) {
        status = "PROSES SELESAI - SILAKAN BUKA PINTU";
        progress = 100;
    }

    String json = "{\"progress\":" + String(progress) +
                  ",\"status\":\"" + status +
                  "\",\"pintu\":" + String(pintuBisaDibuka ? "true" : "false") + "}";

    server.send(200, "application/json", json);
}

void tanganiStart() {
    if (!prosesBerjalan && !pintuBisaDibuka) {
        prosesBerjalan = true;
        pintuBisaDibuka = false;
        waktuMulai = millis();

        digitalWrite(PIN_RELAY_MESIN, RELAY_ON);
    }

    server.sendHeader("Location", "/");
    server.send(303);
}

void tanganiBukaPintu() {
    bukaPintuAction();
    server.sendHeader("Location", "/");
    server.send(303);
}

// ============================================================
// SETUP
// ============================================================
void setup() {
    Serial.begin(115200);

    // Setup GPIO & Relay State awal
    pinMode(PIN_RELAY_MESIN, OUTPUT);
    pinMode(PIN_RELAY_PINTU, OUTPUT);
    digitalWrite(PIN_RELAY_MESIN, RELAY_OFF);
    digitalWrite(PIN_RELAY_PINTU, RELAY_OFF);

    // Setup Tombol
    pinMode(PIN_TOMBOL, INPUT_PULLUP);

    // Setup Servo
    servoPintu.attach(PIN_SERVO);
    servoPintu.write(0);

    // Setup LCD
    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("PROJEK PLASTIKA");
    animasiTeks("SELAMAT DATANG DI PROJEK PLASTIKA", 1, 150);

    // Setup LittleFS
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS gagal!");
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("LittleFS ERROR");
        return;
    }
    Serial.println("LittleFS berhasil!");

    // Setup WiFi
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Connecting WiFi");
    WiFi.begin(SSID_NAME, PASS_WORD);

    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        lcd.print(".");
    }

    Serial.println("\nWiFi connected!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());

    tampilkanIP();

    // Setup WebServer Routes
    server.on("/", tanganiRoot);
    server.on("/style.css", tanganiCSS);
    server.on("/script.js", tanganiJS);
    server.on("/status", tanganiStatus);
    server.on("/start", tanganiStart);
    server.on("/open-door", tanganiBukaPintu);

    server.begin();
    Serial.println("Web server started!");
}

// ============================================================
// MAIN LOOP
// ============================================================
void loop() {
    server.handleClient();

    // Cek Tombol Fisik
    if (digitalRead(PIN_TOMBOL) == LOW && pintuBisaDibuka) {
        bukaPintuAction();
    }

    // Siklus Mesin Running
    if (prosesBerjalan) {
        unsigned long waktuSekarang = millis() - waktuMulai;

        // FASE 1: Peringatan (5 Detik)
        if (waktuSekarang < JEDA_PERINGATAN) {
            lcd.setCursor(0, 0);
            lcd.print("-- PERINGATAN --");
            lcd.setCursor(0, 1);
            lcd.print("JAUHKAN TANGAN! ");
        }
        // FASE 2: Mesin Aktif (20 Detik)
        else if (waktuSekarang < TOTAL_WAKTU) {
            int sisaWaktu = (TOTAL_WAKTU - waktuSekarang) / 1000;
            lcd.setCursor(0, 0);
            lcd.print("  MESIN AKTIF   ");
            lcd.setCursor(0, 1);
            lcd.print("Timer: " + String(sisaWaktu) + " Detik   ");
        }
        // FASE 3: Selesai
        else {
            digitalWrite(PIN_RELAY_MESIN, RELAY_OFF);
            prosesBerjalan = false;
            pintuBisaDibuka = true;

            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("PROSE SELESAI  ");
            lcd.setCursor(0, 1);
            lcd.print("SILAKAN BUKA   ");
        }
    }
}