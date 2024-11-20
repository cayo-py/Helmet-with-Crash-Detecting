#include <PulseSensorPlayground.h>

#define PULSE_SENSOR_PIN 36 // pin VP

PulseSensorPlayground pulseSensor;

void setup() {
  Serial.begin(115200);

  // Atur pulse sensor
  pulseSensor.analogInput(PULSE_SENSOR_PIN);
  pulseSensor.setThreshold(550); // Threshold detak jantung
  pulseSensor.begin();
}

void loop() {
  while (true) {
    getPulseReading();
    delay(500);  // Monitoring interval
  }

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