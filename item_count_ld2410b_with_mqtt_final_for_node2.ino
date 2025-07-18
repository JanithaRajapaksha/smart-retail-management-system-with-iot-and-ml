#include "HX711.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <PubSubClient.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define calibration_factor 1054
#define DOUT 18
#define CLK 19
HX711 scale;

// Wi-Fi & MQTT setup
const char* ssid = "Dialog 4G 909";
const char* password = "F88796d7";
const char* mqtt_server = "192.168.8.135";
// const char* ssid = "SLT_FIBRE";
// const char* password = "47/2.Fbr";
// const char* mqtt_server = "192.168.1.7";

WiFiClient espClient;
PubSubClient client(espClient);

// GPIO pin for status LED
#define LED_PIN 4
#define CONTROL_LED_PIN 2
#define SWITCH_PIN 15  // Rocker switch GPIO

String switchStatus;

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("]: ");
  Serial.println(message);

  if (String(topic) == "lower/led") {
    if (message == "on") {
      digitalWrite(CONTROL_LED_PIN, HIGH);
      Serial.println("💡 LED turned ON via MQTT");
    } else if (message == "off") {
      digitalWrite(CONTROL_LED_PIN, LOW);
      Serial.println("💡 LED turned OFF via MQTT");
    }
  }
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  pinMode(SWITCH_PIN, INPUT_PULLUP);  // Rocker switch with internal pull-up

  pinMode(CONTROL_LED_PIN, OUTPUT);  // LED controlled via MQTT
  digitalWrite(CONTROL_LED_PIN, LOW);  // Start with LED off

  Serial.begin(115200);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);  // Set once before connecting

  while (!client.connected()) {
    Serial.print("Connecting to MQTT...");
    if (client.connect("LD2410_Sensor")) {
      Serial.println("Connected!");
      client.subscribe("lower/led");  // Subscribe once after successful connection
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5s...");
      delay(5000);
    }
  }

  if (WiFi.status() == WL_CONNECTED && client.connected()) {
    digitalWrite(LED_PIN, HIGH); // Indicate success
  }

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

  const int numReadings = 15;
  int counts[numReadings];

  // Collect 15 item count readings
  for (int i = 0; i < numReadings; i++) {
    float weight = scale.get_units();
    Serial.println(weight);
    counts[i] = round(weight / 200);  // Store item count
    Serial.println(counts[i]);
    // delay(100);  // Small delay between readings
  }

  // Compute mode
  int modeCount = 0;
  int mode = counts[0];
  for (int i = 0; i < numReadings; i++) {
    int count = 1;
    for (int j = i + 1; j < numReadings; j++) {
      if (counts[j] == counts[i]) {
        count++;
      }
    }
    if (count > modeCount) {
      modeCount = count;
      mode = counts[i];
    }
  }

  // Serial print
  Serial.print("Final Mode Count: ");
  Serial.println(mode);

  // Publish mode
  String payload = "{\"count\": " + String(mode) + "}";
  client.publish("lower/quantity", payload.c_str());

  // ROCKER SWITCH detection (independent of human presence)
  byte switchState = digitalRead(SWITCH_PIN);
  if (switchState == LOW) {
    Serial.println("🔘 Employee switch activated!");
    client.publish("upper/employee", "{\"employee_present\": true}");
    switchStatus = "Employee Present";
  } else {
    Serial.println("⚪️ Employee switch not activated.");
    client.publish("upper/employee", "{\"employee_present\": false}");
    switchStatus = "No Employee";
  }

  // Update OLED
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.println("Item Count (Mode):");
  display.setTextSize(2);
  display.setCursor(0, 15);
  display.println(mode);
  display.setTextSize(1);
  display.setCursor(0, 40);
  display.print("Switch: ");
  display.println(switchStatus);
  display.display();


  // delay(500);  // Delay before next 15-reading cycle
}


void reconnect() {
  while (!client.connected()) {
    Serial.print("Reconnecting to MQTT...");
    if (client.connect("LD2410_Sensor")) {
      Serial.println("Reconnected!");
      digitalWrite(LED_PIN, HIGH);
      client.subscribe("lower/led");  // <<< Re-subscribe here
    } else {
      Serial.print("Failed, rc=");
      Serial.print(client.state());
      Serial.println(" Trying again in 5s...");
      delay(5000);
    }
  }
}

