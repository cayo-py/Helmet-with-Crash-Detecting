#include <Wire.h>
#include <MPU6050.h>
#include <TinyGPS++.h>
#include <PulseSensorPlayground.h>

// Pin Definitions
#define MPU6050_INT_PIN 19  // GPIO19 untuk interrupt
#define PULSE_SENSOR_PIN 36 // pin VP
#define GPS_RX_PIN 17
#define GPS_TX_PIN 16

MPU6050 mpu;
TinyGPSPlus gps;
HardwareSerial GPS(1);
PulseSensorPlayground pulseSensor;

// Thresholds dan flags
const float THRESHOLD = 15.0;  // Threshold akselerasi dalam m/s²
bool shockDetected = false;
bool gpsLocationFound = false; // Menandakan apakah GPS telah mendapatkan lokasi valid

void setup() {
  Serial.begin(115200);
  GPS.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);

  Wire.begin();
  mpu.initialize();  // Inisialisasi sensor MPU6050

  pinMode(MPU6050_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(MPU6050_INT_PIN), mpuInterrupt, RISING);

  // Konfigurasi interrupt MPU-6050
  mpu.setMotionDetectionThreshold(10); // Sensitivitas motion detection
  mpu.setMotionDetectionDuration(1);   // Durasi deteksi
  mpu.setInterruptMode(MPU6050_INTMODE_ACTIVEHIGH); // Konfigurasi interrupt aktif tinggi
  mpu.setInterruptLatch(MPU6050_INTLATCH_WAITCLEAR); // Interrupt tetap aktif sampai dibersihkan
  mpu.setIntMotionEnabled(true);       // Aktifkan interrupt untuk motion detection

  // Atur pulse sensor
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.setThreshold(550); // Threshold detak jantung
  pulseSensor.begin();

  Serial.println("System Initialized. MPU monitoring active. Other sensors in sleep mode.");
}

void loop() {
  if (!shockDetected) {
    // Pembacaan akselerasi MPU
    int16_t ax, ay, az;
    mpu.getAcceleration(&ax, &ay, &az);

    float ax_mps2 = ax / 16384.0 * 9.81;
    float ay_mps2 = ay / 16384.0 * 9.81;
    float az_mps2 = az / 16384.0 * 9.81;

    float magnitude = sqrt(ax_mps2 * ax_mps2 + ay_mps2 * ay_mps2 + az_mps2 * az_mps2);

    Serial.print("Akselerasi [m/s²]: ");
    Serial.print("X="); Serial.print(ax_mps2, 2);
    Serial.print(", Y="); Serial.print(ay_mps2, 2);
    Serial.print(", Z="); Serial.print(az_mps2, 2);
    Serial.print(" | Magnitude: "); Serial.println(magnitude);

    // Deteksi guncangan
    if (magnitude > THRESHOLD) {
      shockDetected = true;
    }
    delay(500);  // Monitoring interval
  } else {
    handleInterrupt(); // Masuk ke mode interrupt
  }
}

void mpuInterrupt() {
  shockDetected = true;
}

void handleInterrupt() {
  Serial.println("Shock detected! Activating GPS and Pulse Sensor...");
  while (true) { // Tetap dalam interrupt sampai perangkat dinyalakan ulang
    // GPS hanya aktif jika belum mendapatkan lokasi valid
    if (!gpsLocationFound) {
      gpsLocationFound = getGPSLocation();
    }
    else {
      Serial.print("Latitude: ");
      Serial.println(gps.location.lat(), 6);
      Serial.print("Longitude: ");
      Serial.println(gps.location.lng(), 6);
      
      // Pulse sensor selalu membaca data
      getPulseReading();
    }

    delay(500); // Delay untuk memberikan waktu antar pembacaan
  }
}

bool getGPSLocation() {
  Serial.println("Fetching GPS Location...");
  while (GPS.available() > 0) {
    if (gps.encode(GPS.read())) {
      if (gps.location.isValid()) {
        Serial.println("GPS Location Valid.");
        return true; // Lokasi valid ditemukan
      }
    }
  }
  Serial.println("GPS location not valid.");
  return false;
}

void getPulseReading() {
  int pulseValue = pulseSensor.getBeatsPerMinute();
  bool heartBeat = pulseSensor.sawStartOfBeat();
  if (heartBeat) {
    Serial.print("Pulse Detected! BPM: ");
    Serial.println(pulseValue);
  } else {
    Serial.println("Pulse not Detected!");
  }
}
