#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <time.h>
#include "env.h"

#define USE_LM36 false  // Set to false to use DHT11

#if !USE_LM36
#include "dht11_sensor.h"
#endif

// --- CONFIGURATION ---
const char* serverUrl = "http://172.16.4.5:5555/api/roomtemp/update";
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 10 * 3600;
const int daylightOffset_sec = 0;

#define TEMP_SENSOR_PIN A0     // For LM36
#define DHT11_DATA_PIN  D5     // GPIO14 = D5 on NodeMCU

const float TEMP_THRESHOLD = 0.3;                       // Degrees Celsius
const unsigned long HEARTBEAT_INTERVAL = 5 * 60 * 1000; // 5 minutes

float lastTemp = -100.0;
unsigned long lastPostTime = 0;

// --- WIFI SETUP ---
void connectWiFi() {
  Serial.print("Connecting to Wi-Fi");
  WiFi.config(local_ip, gateway, subnet, dns);
  WiFi.begin(ssid, password);
  int retries = 0;
  while (WiFi.status() != WL_CONNECTED && retries++ < 20) {
    delay(500);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n✅ Connected to Wi-Fi");
    Serial.println("IP Address: " + WiFi.localIP().toString());
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    delay(500);
  } else {
    Serial.println("\n❌ Failed to connect to Wi-Fi");
  }
}

// --- SENSOR READINGS ---
float readLM36() {
  int raw = analogRead(TEMP_SENSOR_PIN);
  float voltage = raw * (3.3 / 1023.0);
  return voltage * 100.0;
}

// --- SERVER COMMUNICATION ---
void sendToServer(float temperature, float humidity = -1.0) {
  if (WiFi.status() != WL_CONNECTED) {
    connectWiFi();
    if (WiFi.status() != WL_CONNECTED) return;
  }

  WiFiClient client;
  HTTPClient http;
  http.begin(client, serverUrl);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("X-API-KEY", apiKey);

  String payload = "{";
  payload += "\"location\":\"Room 1\",";
  payload += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  payload += "\"temperature_c\":" + String(temperature, 2) + ",";

#if !USE_LM36
  if (humidity >= 0.0) {
    payload += "\"humidity_percent\":" + String(humidity, 2) + ",";
  } else {
    Serial.println("⚠️ Humidity reading not available.");
  }
#endif

  struct tm timeinfo;
  if (getLocalTime(&timeinfo)) {
    char buf[32];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &timeinfo);
    payload += "\"timestamp\":\"" + String(buf) + "\"";
  } else {
    payload += "\"timestamp\":\"unsynced\"";
  }

  payload += "}";

  int response = http.POST(payload);
  Serial.printf("📡 POST status: %d\n", response);
  if (response > 0) {
    Serial.println("➡️ Payload: " + payload);
  } else {
    Serial.println("❌ POST failed.");
  }
  http.end();
}

// --- MAIN SETUP ---
void setup() {
  Serial.begin(115200);

#if USE_LM36
  pinMode(TEMP_SENSOR_PIN, INPUT);
#else
  dht.begin();
#endif

  connectWiFi();
}

// --- LOOP ---
void loop() {
  float temperature = 0.0;
  float humidity = -1.0;
  bool valid = false;

#if USE_LM36
  temperature = readLM36();
  valid = true;
#else
  valid = readDHTData(temperature, humidity);
#endif

  if (!valid) {
    Serial.println("⚠️ Sensor read failed.");
    delay(2000);
    return;
  }

  unsigned long now = millis();
  bool tempChanged = abs(temperature - lastTemp) >= TEMP_THRESHOLD;
  bool heartbeat = (now - lastPostTime) >= HEARTBEAT_INTERVAL;

  if (tempChanged || heartbeat) {
    sendToServer(temperature, humidity);
    lastTemp = temperature;
    lastPostTime = now;
  }

  delay(2000);
}
