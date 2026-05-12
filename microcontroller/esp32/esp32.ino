#include <WiFi.h>
#include <DHT.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>

/* 
 * IoT AI Bridge - ESP32 Client
 * Monitoring DHT22, Controlling LED via FastAPI & LocalStack
 */

// Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server_ip = "192.168.1.15"; // Ubah ke IP Laptop/Server Anda
const int server_port = 8000;

#define DHTPIN 4
#define DHTTYPE DHT22
#define LED_PIN 2

DHT dht(DHTPIN, DHTTYPE);
WebSocketsClient webSocket;

unsigned long lastMsg = 0;

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      break;
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected to url: /ws/sensor");
      break;
    case WStype_TEXT:
      Serial.printf("[WSc] get text: %s\n", payload);
      
      StaticJsonDocument<200> doc;
      DeserializationError error = deserializeJson(doc, payload);
      
      if (error) {
        Serial.print(F("deserializeJson() failed: "));
        Serial.println(error.f_str());
        return;
      }

      // Action from Backend AI
      int ledStatus = doc["led_control"]; 
      digitalWrite(LED_PIN, ledStatus == 1 ? HIGH : LOW);
      Serial.printf("LED Status updated: %s\n", ledStatus == 1 ? "ON" : "OFF");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  
  // WiFi Setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  dht.begin();
  
  // WebSocket Setup
  webSocket.begin(server_ip, server_port, "/ws/sensor");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
}

void loop() {
  webSocket.loop();

  // Send data every 5 seconds
  if (millis() - lastMsg > 5000) {
    lastMsg = millis();

    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isnan(h) || isnan(t)) {
      Serial.println(F("Failed to read from DHT sensor!"));
      return;
    }

    StaticJsonDocument<128> doc;
    doc["temp"] = t;
    doc["hum"] = h;
    doc["device_id"] = "ESP32-01";

    String output;
    serializeJson(doc, output);
    webSocket.sendTXT(output);
    
    Serial.print("Data Sent: ");
    Serial.println(output);
  }
}