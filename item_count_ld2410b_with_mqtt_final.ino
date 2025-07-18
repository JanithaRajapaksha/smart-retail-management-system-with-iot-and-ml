#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "MyLD2410.h"

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define calibration_factor 1054
#define DOUT 18
#define CLK 19
HX711 scale;

#define sensorSerial Serial1
#define RX_PIN 16
#define TX_PIN 17
#define SERIAL_BAUD_RATE 115200
#define LD2410_BAUD_RATE 256000

// const char* ssid = "SLT_FIBRE";
// const char* password = "47/2.Fbr";
const char* ssid = "Dialog 4G 909";
const char* password = "F88796d7";
const char* mqtt_server = "192.168.8.135";

WiFiClient espClient;
PubSubClient client(espClient);
MyLD2410 sensor(sensorSerial);

const int distanceThreshold = 100;
const int minMovingSignal = 80;
const int minStationarySignal = 60;

#define LED_PIN 4  // Define the LED pin

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(SERIAL_BAUD_RATE);
  sensorSerial.begin(LD2410_BAUD_RATE, SERIAL_8N1, RX_PIN, TX_PIN);
  delay(2000);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  client.setServer(mqtt_server, 1883);
  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("LD2410_Sensor")) {
      Serial.println("Connected!");
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5s...");
      delay(5000);
    }
  }

  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    digitalWrite(LED_PIN, HIGH); // Turn on LED if both WiFi and MQTT are connected
  }

  if (!sensor.begin()) {
    Serial.println("Failed to communicate with the sensor.");
    while (true) {}
  }
  sensor.enhancedMode();

  if (!display.begin(SSD1306_PAGEADDR, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 0);
  display.println(F("HX711 Scale Setup"));
  display.display();
  delay(2000);

  scale.begin(DOUT, CLK);
  scale.set_scale(calibration_factor);
  scale.tare();
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Weight measurement and item count
  float weight = scale.get_units();
  int itemCount = round(weight / 100.0);  // Proper rounding

  Serial.print("Weight: ");
  Serial.print(weight);
  Serial.print(" g -> Count: ");
  Serial.println(itemCount);

  // Publish weight as count
  String weightPayload = "{\"count\": " + String(itemCount) + "}";
  client.publish("upper/quantity", weightPayload.c_str());

  // Default presence values
  String presenceStatus = "No human";
  String presencePayload = "{\"detected\": false}";

  // Presence detection
  if (sensor.check() == MyLD2410::Response::DATA) {
    int distance = sensor.detectedDistance();
    int movingSignal = sensor.movingTargetSignal();
    int stationarySignal = sensor.stationaryTargetSignal();

    if (sensor.presenceDetected() && distance <= distanceThreshold) {
      if (sensor.movingTargetDetected() && movingSignal > minMovingSignal) {
        Serial.println("🟢 Human detected near the table!");
        presencePayload = "{\"detected\": true, \"type\": \"moving\"}";
        presenceStatus = "Human (moving)";
      } else if (sensor.stationaryTargetDetected() && stationarySignal > minStationarySignal) {
        Serial.println("🟢 Stationary human detected near the table!");
        presencePayload = "{\"detected\": true, \"type\": \"stationary\"}";
        presenceStatus = "Human (still)";
      } else {
        Serial.println("🟡 Weak signal. No valid human presence.");
      }
    } else {
      Serial.println("🔴 No human detected.");
    }
  } else {
    Serial.println("⚠️ No data from LD2410 sensor. Assuming no presence.");
  }

  client.publish("upper/presence", presencePayload.c_str());

  // Update OLED with count and presence
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Item Count:");
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.println(itemCount);

  display.setTextSize(1);
  display.setCursor(0, 45);
  display.print("Presence: ");
  display.println(presenceStatus);
  display.display();

  delay(100);
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Reconnecting to MQTT...");
    if (client.connect("LD2410_Sensor")) {
      Serial.println("Reconnected!");
      digitalWrite(LED_PIN, HIGH); // Ensure LED is turned on after reconnection
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5s...");
      delay(5000);
    }
  }
}
