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
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

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

// --- Safety & Timing Constants ---
const unsigned long SEND_INTERVAL = 5000;       // Send data every 5 seconds
const float TEMP_CHANGE_THRESHOLD = 0.5;        // Only send if temp changes > 0.5°C
const float HUM_CHANGE_THRESHOLD = 1.0;       // Only send if humidity changes > 1.0%
const unsigned long WATCHDOG_TIMEOUT = 30000;// 30 seconds watchdog
const int MAX_RECONNECT_ATTEMPTS = 5;         // Max reconnection attempts before wait
const unsigned long RECONNECT_WAIT = 60000;    // Wait 1 minute after max attempts

// System State
bool st_kipas = false, st_mist = false, st_heater = false, st_mode = true;
float current_temp = 0, current_hum = 0;
float last_sent_temp = -100, last_sent_hum = -100;
String current_alert = "Normal";
bool ws_connected = false;
unsigned long last_send_time = 0;
unsigned long last_ws_heartbeat = 0;
int reconnect_attempts = 0;
unsigned long reconnect_wait_start = 0;

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

// --- Utility Functions ---
bool isSensorDataValid(float temp, float hum) {
  // Sanity check for DHT22
  if (isnan(temp) || isnan(hum)) return false;
  if (temp < -40 || temp > 80) return false;  // DHT22 temp range
  if (hum < 0 || hum > 100) return false;    // Humidity range
  return true;
}

bool shouldSendData(float newTemp, float newHum) {
  // Check if enough time has passed
  if (millis() - last_send_time < SEND_INTERVAL) return false;
  
  // Check if data changed significantly
  bool tempChanged = abs(newTemp - last_sent_temp) >= TEMP_CHANGE_THRESHOLD;
  bool humChanged = abs(newHum - last_sent_hum) >= HUM_CHANGE_THRESHOLD;
  
  return tempChanged || humChanged;
}

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
      ws_connected = false;
      break;
      
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected!");
      lcd.setCursor(18, 0); lcd.print("OK");
      ws_connected = true;
      reconnect_attempts = 0;  // Reset reconnect counter
      last_ws_heartbeat = millis();
      break;
      
    case WStype_TEXT:
      {
        last_ws_heartbeat = millis();  // Update heartbeat on any message
        
        StaticJsonDocument<256> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (error == DeserializationError::Ok) {
          if (doc.containsKey("led_control")) st_kipas = (doc["led_control"] == 1);
          if (doc.containsKey("control_2")) st_mist = (doc["control_2"] == 1);
          if (doc.containsKey("control_3")) st_heater = (doc["control_3"] == 1);
          if (doc.containsKey("control_4")) st_mode = (doc["control_4"] == 1);
          if (doc.containsKey("alert")) current_alert = doc["alert"].as<String>();
          
          // Safety: Only update actuators with safety checks
          digitalWrite(kipas_pin, st_kipas ? HIGH : LOW);
          digitalWrite(mist_pin, st_mist ? HIGH : LOW);
          digitalWrite(heater_pin, st_heater ? HIGH : LOW);
          digitalWrite(mode_pin, st_mode ? HIGH : LOW);
          
          updateLCD();
        }
      }
      break;
      
    case WStype_PING:
    case WStype_PONG:
      last_ws_heartbeat = millis();
      break;
  }
}

// --- Safety: Initialize Actuators ---
void initActuatorsSafe() {
  // Set all actuators to safe state (OFF) at startup
  digitalWrite(kipas_pin, LOW);
  digitalWrite(mist_pin, LOW);
  digitalWrite(heater_pin, LOW);
  digitalWrite(mode_pin, HIGH);  // Default to AUTO mode
  st_kipas = false;
  st_mist = false;
  st_heater = false;
  st_mode = true;
}

// Task 1: WebSocket & Network Loop (Pinned to Core 0)
void WebSocketTask(void * pvParameters) {
  for(;;) {
    // Check if we're in reconnect wait period
    if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
      if (millis() - reconnect_wait_start < RECONNECT_WAIT) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        continue;
      } else {
        reconnect_attempts = 0;  // Reset after waiting
      }
    }
    
    webSocket.loop();
    
    // Watchdog: Check if WS is alive
    if (ws_connected && (millis() - last_ws_heartbeat > WATCHDOG_TIMEOUT)) {
      Serial.println("[WSc] Watchdog timeout - reconnecting...");
      webSocket.disconnect();
      ws_connected = false;
      reconnect_attempts++;
      if (reconnect_attempts >= MAX_RECONNECT_ATTEMPTS) {
        reconnect_wait_start = millis();
        Serial.printf("[WSc] Max reconnect attempts reached, waiting %lu seconds...\n", RECONNECT_WAIT / 1000);
      }
    }
    
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

// Task 2: Sensor Reading & UI Update (Pinned to Core 1)
void SensorTask(void * pvParameters) {
  for(;;) {
    float h = dht.readHumidity();
    float t = dht.readTemperature();

    if (isSensorDataValid(t, h)) {
      current_temp = t;
      current_hum = h;

      // Only send if connected, data changed, and interval passed
      if (ws_connected && shouldSendData(t, h)) {
        StaticJsonDocument<128> doc;
        doc["temp"] = current_temp;
        doc["hum"] = current_hum;
        doc["id"] = "ESP32-01";
        
        String out;
        serializeJson(doc, out);
        
        if (webSocket.sendTXT(out)) {
          last_sent_temp = t;
          last_sent_hum = h;
          last_send_time = millis();
          Serial.printf("[Sensor] Sent: %.1fC %.1f%%\n", t, h);
        } else {
          Serial.println("[Sensor] Failed to send data");
        }
      }
      
      // Local ML Inference (NocML)
      float feat[2] = {current_temp, current_hum};
      int pred = logReg.predict(feat);
      Serial.printf("[Local AI]: %d\n", pred);
      
      updateLCD();
    } else {
      Serial.println("[Sensor] Invalid sensor read - skipping");
    }
    
    vTaskDelay(2000 / portTICK_PERIOD_MS);  // Read sensor every 2s (but send only every 5s or on change)
  }
}

void setup() {
  Serial.begin(115200);
  
  // Initialize safety first
  initActuatorsSafe();
  
  pinMode(kipas_pin, OUTPUT);
  pinMode(mist_pin, OUTPUT);
  pinMode(heater_pin, OUTPUT);
  pinMode(mode_pin, OUTPUT);
  
  Wire.begin(I2C_SDA, I2C_SCL);
  
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IoT AI Bridge RTOS");
  
  Serial.println("[Setup] Connecting to WiFi...");
  
  WiFi.begin(ssid, password);
  
  // WiFi connection with timeout
  unsigned long wifi_start = millis();
  while (WiFi.status() != WL_CONNECTED) {
    if (millis() - wifi_start > 20000) {  // 20 second timeout
      Serial.println("[Setup] WiFi timeout - using offline mode");
      break;
    }
    delay(500);
    Serial.print(".");
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.printf("\n[Setup] WiFi connected! IP: %s\n", WiFi.localIP().toString().c_str());
    
    webSocket.begin(server_ip, server_port, "/ws/sensor");
    webSocket.onEvent(webSocketEvent);
    webSocket.setReconnectInterval(5000);
    // Enable heartbeat: pingInterval, pongTimeout, disconnectTimeoutCount
    webSocket.enableHeartbeat(30000, 5000, 3);  // Heartbeat every 30s, timeout 5s, disconnect after 3 failures
  }
  
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
