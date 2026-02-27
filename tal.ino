#include <WiFi.h>
#include <PubSubClient.h>
#include "DHT.h"

// --- Pins ---
const int ddht = 14; 
const int relay = 12; 
const int buttonPin = 13; 

#define DHTTYPE DHT22
DHT dht(ddht, DHTTYPE);

// --- NETPIE Config ---
const char* ssid = "patarn_2.4G";
const char* password = "Tt0851247818";
const char* mqtt_server = "broker.netpie.io";
const char* mqtt_Client = "5d54fe1d-e1a5-4192-9583-d9c1a306558a";
const char* mqtt_username = "e7S6j1eZxhDgkt7rZMokANHDbQmyMi6i";
const char* mqtt_password = "nV5WzefQKSUD9m1r8P3f5XdtjGJpY3ZX";

WiFiClient espClient;
PubSubClient client(espClient);

// Variables
bool autoMode = true; 
bool lastButtonState = HIGH;
unsigned long lastMsg = 0;
unsigned long lastDebounceTime = 0;
unsigned long debounceDelay = 50; 

// --- Hysteresis Settings ---
float upperLimit = 85.0; // จุดที่เริ่มเปิดปั๊ม
float lowerLimit = 75.0; // จุดที่สั่งปิดปั๊ม

void setup() {
  pinMode(relay, OUTPUT);
  digitalWrite(relay, HIGH); // ปิด Relay (Active Low)
  pinMode(buttonPin, INPUT_PULLUP); 
  
  Serial.begin(115200);
  dht.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.println("\nWiFi connected");

  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);
}

void handleButton() {
  int reading = digitalRead(buttonPin);
  if (reading != lastButtonState) { lastDebounceTime = millis(); }

  if ((millis() - lastDebounceTime) > debounceDelay) {
    if (reading == LOW && lastButtonState == HIGH) {
      autoMode = !autoMode; 
      Serial.print("Mode: "); Serial.println(autoMode ? "AUTO" : "MANUAL");
      if (!autoMode) digitalWrite(relay, HIGH); 
    }
  }
  lastButtonState = reading;
}

void callback(char* topic, byte* payload, unsigned int length) {
  String message;
  for (int i = 0; i < length; i++) { message += (char)payload[i]; }
  
  if (String(topic) == "@msg/relay") {
    if (message == "on") { digitalWrite(relay, LOW); autoMode = false; }
    else if (message == "off") { digitalWrite(relay, HIGH); autoMode = false; }
    else if (message == "auto") { autoMode = true; }
  }
}

void reconnect() {
  while (!client.connected()) {
    if (client.connect(mqtt_Client, mqtt_username, mqtt_password)) {
      client.subscribe("@msg/#");
    } else { delay(5000); }
  }
}

void loop() {
  if (!client.connected()) reconnect();
  client.loop();
  handleButton();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h)) {
      if (autoMode) {
        // --- ส่วนของ Hysteresis Logic ---
        if (h > upperLimit) {
          digitalWrite(relay, LOW);  // เปิดปั๊มเมื่อชื้นเกินไป
        } else if (h < lowerLimit) {
          digitalWrite(relay, HIGH); // ปิดปั๊มเมื่อความชื้นลดลงมาถึงจุดที่พอใจ
        }
        // ถ้าอยู่ระหว่าง 75-85 จะคงสถานะเดิมไว้ (ไม่เปลี่ยนไปมา)
      }
    }

    int rStatus = digitalRead(relay);
    String payload = "{\"data\":{\"Humidity\":" + String(h) + 
                     ",\"Temperature\":" + String(t) + 
                     ",\"relay\":" + String(rStatus) + 
                     ",\"auto\":" + String(autoMode) + "}}";
    
    client.publish("@shadow/data/update", payload.c_str());
    Serial.println(payload);
  }
}