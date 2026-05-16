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
 */

// Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server_ip = "192.168.8.129"; 
const int server_port = 8000;

#define DHTPIN 25
#define DHTTYPE DHT22

// I2C LCD Pins
#define I2C_SDA 21
#define I2C_SCL 22

// Control Pins
int control_1_pin = 14; // Kipas
int control_2_pin = 16; // Mist Maker
int control_3_pin = 27; // Heater
int control_4_pin = 17; // Mode

// Status Component
bool st_kipas = false, st_mist = false, st_heater = false, st_mode = false;
float current_temp = 0, current_hum = 0;
String current_alert = "Normal";

DHT dht(DHTPIN, DHTTYPE);
WebSocketsClient webSocket;
LiquidCrystal_I2C lcd(0x27, 20, 4);

// NocML Configuration
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
      StaticJsonDocument<256> doc;
      if (deserializeJson(doc, payload) == DeserializationError::Ok) {
        if (doc.containsKey("led_control")) st_kipas = (doc["led_control"] == 1);
        if (doc.containsKey("control_2")) st_mist = (doc["control_2"] == 1);
        if (doc.containsKey("control_3")) st_heater = (doc["control_3"] == 1);
        if (doc.containsKey("control_4")) st_mode = (doc["control_4"] == 1);
        if (doc.containsKey("alert")) current_alert = doc["alert"].as<String>();
        
        digitalWrite(control_1_pin, st_kipas ? HIGH : LOW);
        digitalWrite(control_2_pin, st_mist ? HIGH : LOW);
        digitalWrite(control_3_pin, st_heater ? HIGH : LOW);
        digitalWrite(control_4_pin, st_mode ? HIGH : LOW);
        
        updateLCD();
      }
      break;
  }
}

// Task 1: WebSocket & Network (Core 0)
void WebSocketTask(void * pvParameters) {
  for(;;) {
    webSocket.loop();
    vTaskDelay(1 / portTICK_PERIOD_MS); // Yield to OS
  }
}

// Task 2: Sensors & UI (Core 1)
void SensorTask(void * pvParameters) {
  for(;;) {
    current_hum = dht.readHumidity();
    current_temp = dht.readTemperature();

    if (!isnan(current_hum) && !isnan(current_temp)) {
      StaticJsonDocument<128> doc;
      doc["temp"] = current_temp;
      doc["hum"] = current_hum;
      doc["device_id"] = "ESP32-01";
      String out;
      serializeJson(doc, out);
      webSocket.sendTXT(out);
      
      // Local Inference
      float feat[2] = {current_temp, current_hum};
      int pred = logReg.predict(feat);
      Serial.printf("Sensor: %.1fC %.1f%% | Local AI: %d\n", current_temp, current_hum, pred);
      
      updateLCD();
    }
    vTaskDelay(5000 / portTICK_PERIOD_MS); // Wait 5 seconds
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(control_1_pin, OUTPUT);
  pinMode(control_2_pin, OUTPUT);
  pinMode(control_3_pin, OUTPUT);
  pinMode(control_4_pin, OUTPUT);
  
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

  // Create Tasks
  xTaskCreatePinnedToCore(WebSocketTask, "WS", 8192, NULL, 2, &TaskWS, 0);
  xTaskCreatePinnedToCore(SensorTask, "Sensor", 8192, NULL, 1, &TaskSensors, 1);
}

void loop() {
  // Empty - handled by RTOS tasks
}