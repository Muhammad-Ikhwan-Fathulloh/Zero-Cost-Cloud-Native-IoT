//Check Board ESP
#ifdef ESP8266
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <WiFi.h>
#include <ESPmDNS.h>
#else
#error "Board not found"
#endif

#include <DHT.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <NocML.h>
#include <LiquidCrystal_I2C.h>
#include <Wire.h>

/* 
 * IoT AI Bridge - ESP32 Client (FreeRTOS Version)
 * Library: https://github.com/Nocturnailed-Community/NocML
 * Features: Dual-Core Execution, Real-time LCD, Local ML Inference
 */

// --- Configuration ---
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server_ip = "192.168.8.129"; 
const int server_port = 8000;

#define DHTPIN 25
#define DHTTYPE DHT22

// I2C LCD Pins (ESP32 Default)
#define I2C_SDA 21
#define I2C_SCL 22

// Control Pins (Actuators)
const int kipas_pin = 14; 
const int mist_pin = 16; 
const int heater_pin = 27; 
const int mode_pin = 17;

// System State
bool st_kipas = false, st_mist = false, st_heater = false, st_mode = true;
float current_temp = 0, current_hum = 0;
String current_alert = "Normal";

DHT dht(DHTPIN, DHTTYPE);
WebSocketsClient webSocket;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// NocML Local Inference Configuration
const float weights[2] = {0.85, 0.42};
const float bias = -25.5;
NocML::LogisticRegression logReg(2, weights, bias);

// Task Handles
TaskHandle_t TaskWS;
TaskHandle_t TaskSensors;

void updateLCD() {
  lcd.setCursor(0, 1);
  lcd.print("T:"); lcd.print(current_temp, 1);
  lcd.print("C H:"); lcd.print(current_hum, 1); lcd.print("%  ");
  
  lcd.setCursor(0, 2);
  lcd.print("K:"); lcd.print(st_kipas ? "ON " : "OFF");
  lcd.setCursor(11, 2);
  lcd.print("M:"); lcd.print(st_mist ? "ON " : "OFF");
  
  lcd.setCursor(0, 3);
  lcd.print("H:"); lcd.print(st_heater ? "ON " : "OFF");
  lcd.setCursor(11, 3);
  lcd.print("Mod:"); lcd.print(st_mode ? "AUTO" : "MAN ");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      lcd.setCursor(18, 0); lcd.print("DC");
      break;
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected!");
      lcd.setCursor(18, 0); lcd.print("OK");
      break;
    case WStype_TEXT:
      {
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error == DeserializationError::Ok) {
          if (doc.containsKey("led_control")) st_kipas = (doc["led_control"] == 1);
          if (doc.containsKey("control_2")) st_mist = (doc["control_2"] == 1);
          if (doc.containsKey("control_3")) st_heater = (doc["control_3"] == 1);
          if (doc.containsKey("control_4")) st_mode = (doc["control_4"] == 1);
          if (doc.containsKey("alert")) current_alert = doc["alert"].as<String>();
          
          digitalWrite(kipas_pin, st_kipas ? HIGH : LOW);
          digitalWrite(mist_pin, st_mist ? HIGH : LOW);
          digitalWrite(heater_pin, st_heater ? HIGH : LOW);
          digitalWrite(mode_pin, st_mode ? HIGH : LOW);
          
          updateLCD();
        }
      }
      break;
  }
}

// Task 1: WebSocket & Network Loop (Pinned to Core 0)
void WebSocketTask(void * pvParameters) {
  for(;;) {
    webSocket.loop();
    vTaskDelay(1 / portTICK_PERIOD_MS); 
  }
}

// Task 2: Sensor Reading & UI Update (Pinned to Core 1)
void SensorTask(void * pvParameters) {
  for(;;) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (!isnan(h) && !isnan(t)) {
      current_temp = t;
      current_hum = h;

      StaticJsonDocument<128> doc;
      doc["temp"] = current_temp;
      doc["hum"] = current_hum;
      doc["id"] = "ESP32-01"; // Unified with backend 'id' key
      
      String out;
      serializeJson(doc, out);
      webSocket.sendTXT(out);
      
      // Local ML Inference (NocML)
      float feat[2] = {current_temp, current_hum};
      int pred = logReg.predict(feat);
      Serial.printf("Sensor: %.1fC %.1f%% | Local AI: %d\n", current_temp, current_hum, pred);
      
      updateLCD();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS); 
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(kipas_pin, OUTPUT);
  pinMode(mist_pin, OUTPUT);
  pinMode(heater_pin, OUTPUT);
  pinMode(mode_pin, OUTPUT);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IoT AI Bridge RTOS");
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  webSocket.begin(server_ip, server_port, "/ws/sensor");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  dht.begin();

  // Create RTOS Tasks
  xTaskCreatePinnedToCore(
    WebSocketTask, "WS_Task", 8192, NULL, 2, &TaskWS, 0
  );
  xTaskCreatePinnedToCore(
    SensorTask, "Sensor_Task", 8192, NULL, 1, &TaskSensors, 1
  );
}

void loop() {
  // Empty - tasks are handled by FreeRTOS
}
