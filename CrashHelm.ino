#include <Wire.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <PulseSensorPlayground.h>

// Koneksi Wi-Fi, pastikan sesuai dengan yang ingin digunakan.
const char* ssid = "SSID";
const char* password = "PASS";

// Token Telegram
/* 
Langkah mendapatkan BOTtoken di Telegram:
    1. cari @BotFather di Telegram
    2. klik START
    3. ketik /newbot 
    4. masukkan nama bot yang diinginkan
    5. ketikkan username yang diinginkan
    6. copy bagian HTTP API yang diberikan oleh bot 
       dan paste pada bagian yang ada program ini. 
Langkah mendapatkan chat id di Telegram:
    1. cari @myidbot di Telegram
    2. klik START
    3. ketik /getid
    4. copy id yang diberikan dan paste pada bagian yang ada program ini.
*/
#define BOTtoken "[BOTtoken]"
#define CHAT_ID "CHAT_ID"

// Definisi Pin
#define PULSE_SENSOR_PIN 36 // Pin VP
#define GPS_RX_PIN 16
#define GPS_TX_PIN 17 
#define LED 2 // Hanya indikator (LED built in esp32) kalo: > threshold akselerasi dan gps retrying

// Objek sensor dan komunikasi
MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial GPS(1);
PulseSensorPlayground pulseSensor;

// Thresholds
const float MPU_Threshold = 35000; // Threshold akselerasi

void setup() {
  Serial.begin(115200);

  pinMode(LED, OUTPUT);

  // Cek status deep sleep
  if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_UNDEFINED) {
    Serial.println("Device waking up from normal startup.");
  } else {
    ESP.restart(); // Restart perangkat
    Serial.println("Device waking up from deep sleep.");
  }

  // Koneksi Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println(F("Connecting to WiFi..."));
  }
  Serial.println(F("Connected to WiFi."));

  // Inisialisasi semua sensor (GPS, MPU, Pulse)
  GPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
  Wire.begin();
  mpu.initialize();
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.setThreshold(550); // Threshold pulse
  pulseSensor.begin();

  Serial.println(F("System Initialized. MPU monitoring active.")); // Awal program, MPU saja yang aktif
}

void loop() {
  static bool shockDetected = false; // Indikator penanda kalau terdeteksi guncangan oleh MPU

  if (!shockDetected) {
    // Baca akselerasi
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    // Hitung magnitudo akselerasi
    float magnitude = sqrt(ax * ax + ay * ay + az * az);
    Serial.println(magnitude);
    // Deteksi guncangan
    if (magnitude > MPU_Threshold) {
      digitalWrite(LED, HIGH);
      delay(250);
      digitalWrite(LED, LOW);
      shockDetected = true;
    }
    delay(500);
  } else { // Ketika magnitude guncangan dari MPU > threshold, akan menjalankan sensor lain. MPU berhenti.
    handleShock();
  }
}

void handleShock() {
  Serial.println(F("Shock detected!"));

  // Ambil lokasi GPS
  while (!getGPSLocation()) { // Ketika GPS belum mendapatkan data Valid.
    Serial.println(F("Retrying GPS location..."));
    digitalWrite(LED, HIGH);
    delay(250);
    digitalWrite(LED, LOW);
  }

  // Kirim pesan lokasi
  float latitude = gps.location.lat();
  float longitude = gps.location.lng();
  if (sendTelegramMessage(latitude, longitude, nullptr, true)) { // `true` untuk mengirim pesan lokasi
    Serial.println(F("GPS location sent successfully."));
  }

  // Ambil dan kirim pembacaan pulse
  int pulseCount = 0;
  while (pulseCount < 10) { // Kirim satu-satu sampai 10 kali.
    int pulseValue = getPulseReading();
    if (pulseValue > 0) { // Hanya kirim jika pembacaan valid
      if (sendTelegramMessage(0, 0, &pulseValue, false)) { // Kirim data pulse
        Serial.println(F("Pulse reading sent successfully."));
        pulseCount++;
      }
    }
  }
  stopProgram();
}

bool getGPSLocation() {
  Serial.println("Fetching GPS Location...");
  unsigned long startTime = millis(); // Catat waktu mulai
  const unsigned long timeout = 5000; // Timeout 5 detik

  while (millis() - startTime < timeout) { // Tunggu hingga timeout
    while (GPS.available() > 0) {
      char c = GPS.read();
      if (gps.encode(c)) { // Proses data GPS
        if (gps.location.isValid()) { // Lokasi ditemukan
          Serial.println(F("Valid location detected"));
          return true;
        }
      }
    }
    delay(50); // Jeda kecil untuk menunggu data berikutnya
  }

  Serial.println("GPS location not valid within timeout.");
  return false; // Lokasi tidak ditemukan dalam waktu yang diberikan
}

int getPulseReading() {
  if (pulseSensor.sawStartOfBeat()) {
    int pulseValue = pulseSensor.getBeatsPerMinute();
    if (pulseValue >= 60 && pulseValue <= 120) {
      Serial.println(F("Valid pulse detected"));
      return pulseValue;
    }
  }
  return -1; // Pembacaan tidak valid, tidak kirim data.
}

bool sendTelegramMessage(float latitude, float longitude, int* pulseValue, bool isLocation) {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure client;
  client.setInsecure(); // Untuk sertifikat TLS

  String url = String("https://api.telegram.org/bot") + BOTtoken + "/sendMessage";
  String message;

  if (isLocation) {
    // Pesan lokasi GPS
    message = "Emergency Alert! 🚨\nLokasi: https://www.google.com/maps?q=" +
              String(latitude, 6) + "," + String(longitude, 6) +
              "\nStay safe!";
  } else if (pulseValue != nullptr) {
    // Pesan untuk hasil pulse
    message = "Pulse Data Reading:\n";
    message += "Current Pulse: " + String(*pulseValue) + " BPM\n";
  }

  String payload = "{\"chat_id\":\"" + String(CHAT_ID) + "\",\"text\":\"" + message + "\"}";

  if (!client.connect("api.telegram.org", 443)) {
    Serial.println(F("Connection to Telegram failed."));
    return false;
  }

  client.println("POST /bot" + String(BOTtoken) + "/sendMessage HTTP/1.1");
  client.println("Host: api.telegram.org");
  client.println("Content-Type: application/json");
  client.println("Content-Length: " + String(payload.length()));
  client.println();
  client.println(payload);

  unsigned long startTime = millis();
  while (client.connected() || client.available()) {
    if (client.available()) {
      return true; // Pesan berhasil terkirim
    }
    if (millis() - startTime > 5000) break; // Timeout 5 detik
  }
  return false; // Jika tidak ada respons atau gagal
}

void stopProgram() {
  Serial.println(F("Entering deep sleep..."));
  delay(2000); // Waktu untuk log terakhir
  esp_deep_sleep_start();
}
