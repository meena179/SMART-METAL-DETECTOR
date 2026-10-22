#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
// OLED configuration
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define OLED_ADDRESS 0x3C
// ESP32 GPIO pins
#define OLED_SDA 21
#define OLED_SCL 22
#define SENSOR_PIN 27
#define BUZZER_PIN 25
// Detection settings
const unsigned long SAMPLE_TIME_MS = 250;
const unsigned long DISPLAY_TIME_MS = 150;
const unsigned long CALIBRATION_TIME_MS = 3000;
const float DETECTION_THRESHOLD_PERCENT = 15.0f;
Adafruit_SSD1306 display(
  SCREEN_WIDTH,
  SCREEN_HEIGHT,
  &Wire,
  OLED_RESET);
unsigned long lastSampleTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long pulseCount = 0;
unsigned long baselinePulseCount = 0;
float signalPercent = 0.0f;
bool metalDetected = false;
bool calibrated = false;
void drawScreen(
  unsigned long frequency,
  float signal,
  bool detected,
  bool isCalibrated) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(12, 0);
  display.println(F("ESP32 METAL DETECTOR"));
  display.drawLine(0, 11, 127, 11, SSD1306_WHITE);
  display.setCursor(0, 17);
  display.print(F("Frequency: "));
  display.print(frequency);
  display.println(F(" Hz"));
  display.setCursor(0, 29);
  display.print(F("Signal: "));
  display.print(signal, 1);
  display.println(F("%"));
  // Signal bar
  int barWidth = (int)(signal * 1.2f);
  if (barWidth < 0) {
    barWidth = 0;
  }
  if (barWidth > 120) {
    barWidth = 120;
  }
  display.drawRect(3, 42, 122, 10, SSD1306_WHITE);
  if (barWidth > 0) {
    display.fillRect(4, 43, barWidth, 8, SSD1306_WHITE);
  }
  display.setCursor(0, 56);
  if (!isCalibrated) {
    display.print(F("Calibrating..."));
  } else if (detected) {
    display.print(F("METAL DETECTED!"));
  } else {
    display.print(F("Scanning..."));
  }
  display.display();
}
void setup() {
  Serial.begin(115200);
  pinMode(SENSOR_PIN, INPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);
  Wire.begin(OLED_SDA, OLED_SCL);
  if (!display.begin(
        SSD1306_SWITCHCAPVCC,
        OLED_ADDRESS)) {
    Serial.println(F("OLED not found. Check wiring/address."));
    while (true) {
      delay(100);
    }
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.println(F("Metal Detector"));
  display.println(F("Starting..."));
  display.display();
  delay(1000);
  // Establish baseline with the coil away from metal.
  unsigned long calibrationStart = millis();
  unsigned long calibrationEdges = 0;
  int previousState = digitalRead(SENSOR_PIN);
  while (millis() - calibrationStart < CALIBRATION_TIME_MS) {
    int currentState = digitalRead(SENSOR_PIN);
    if (currentState == HIGH && previousState == LOW) {
      calibrationEdges++;
    }
    previousState = currentState;
    delayMicroseconds(50);
  }
  baselinePulseCount = calibrationEdges;
  calibrated = (baselinePulseCount > 0);
  if (!calibrated) {
    Serial.println(
      F("No sensor pulses detected. Check the sensing circuit."));
  }
  lastSampleTime = millis();
  lastDisplayTime = millis();
}
void loop() {
  static int previousState = LOW;
  int currentState = digitalRead(SENSOR_PIN);
  if (currentState == HIGH && previousState == LOW) {
    pulseCount++;
  }
  previousState = currentState;
  unsigned long now = millis();
  if (now - lastSampleTime >= SAMPLE_TIME_MS) {
    unsigned long elapsed = now - lastSampleTime;
    // Convert pulse count to frequency in Hz.
    unsigned long frequency =
      (pulseCount * 1000UL) / elapsed;
    pulseCount = 0;
    lastSampleTime = now;
    if (calibrated && baselinePulseCount > 0) {
      float baselineFrequency =
        (baselinePulseCount * 1000.0f) / CALIBRATION_TIME_MS;
      float difference =
        (float)frequency - baselineFrequency;
      if (difference < 0) {
        difference = -difference;
      }
      float percent =
        (difference / baselineFrequency) * 100.0f;
      if (percent > 100.0f) {
        percent = 100.0f;
      }
      signalPercent = percent;
      metalDetected =
        (signalPercent >= DETECTION_THRESHOLD_PERCENT);
    } else {
      signalPercent = 0.0f;
      metalDetected = false;
    }
    digitalWrite(
      BUZZER_PIN,
      metalDetected ? HIGH : LOW);
    Serial.print(F("Frequency: "));
    Serial.print(frequency);
    Serial.print(F(" Hz, Signal: "));
    Serial.print(signalPercent, 1);
    Serial.print(F("%, Metal: "));
    Serial.println(metalDetected ? F("YES") : F("NO"));
  }
  if (now - lastDisplayTime >= DISPLAY_TIME_MS) {
    unsigned long frequency =
      (pulseCount * 1000UL) / SAMPLE_TIME_MS;
    drawScreen(
      frequency,
      signalPercent,
      metalDetected,
      calibrated);
    lastDisplayTime = now;
  }
}
