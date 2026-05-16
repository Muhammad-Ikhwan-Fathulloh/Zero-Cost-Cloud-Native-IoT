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

#include <LiquidCrystal_I2C.h>
#include <Wire.h>

/* 
 * IoT AI Bridge - ESP32 Client
 * Monitoring DHT22, Controlling Components via FastAPI & LocalStack
 */

// Configuration
const char* ssid = "YOUR_WIFI_SSID";
const char* password = "YOUR_WIFI_PASSWORD";
const char* server_ip = "192.168.1.15"; // Ubah ke IP Laptop/Server Anda
const int server_port = 8000;

#define DHTPIN 25
#define DHTTYPE DHT22

// I2C LCD Pins
#define I2C_SDA 21
#define I2C_SCL 22

//Control Pin
int control_1_pin = 14; //Kipas
int control_2_pin = 16; //Mist Maker
int control_3_pin = 27; //Heater
int control_4_pin = 17; //Mode

// Status Component
bool st_kipas = false;
bool st_mist = false;
bool st_heater = false;
bool st_mode = false;

DHT dht(DHTPIN, DHTTYPE);
WebSocketsClient webSocket;
LiquidCrystal_I2C lcd(0x27, 20, 4);

unsigned long lastMsg = 0;

void updateLCDStatus() {
  // Hanya update baris 2 & 3 agar tidak berkedip
  lcd.setCursor(0, 2);
  lcd.print("Kipas: ");
  lcd.print(st_kipas ? "ON " : "OFF");
  
  lcd.setCursor(11, 2);
  lcd.print("Mist:");
  lcd.print(st_mist ? "ON " : "OFF");
  
  lcd.setCursor(0, 3);
  lcd.print("Heat : ");
  lcd.print(st_heater ? "ON " : "OFF");

  lcd.setCursor(11, 3);
  lcd.print("Mode:");
  lcd.print(st_mode ? "ON " : "OFF");
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {
  switch(type) {
    case WStype_DISCONNECTED:
      Serial.println("[WSc] Disconnected!");
      lcd.setCursor(18, 0);
      lcd.print("DC"); // Status Terputus
      break;
    case WStype_CONNECTED:
      Serial.println("[WSc] Connected to url: /ws/sensor");
      lcd.setCursor(18, 0);
      lcd.print("OK"); // Status Terhubung
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

      // Action from Backend AI (Currently mapped to control_1_pin / Kipas)
      if (doc.containsKey("led_control")) {
        int control1Status = doc["led_control"]; 
        st_kipas = (control1Status == 1);
        digitalWrite(control_1_pin, st_kipas ? HIGH : LOW);
        Serial.printf("Kipas (Control 1) Status updated: %s\n", st_kipas ? "ON" : "OFF");
      }
      
      // Optional: Handle other controls if backend sends them in the future
      if (doc.containsKey("control_2")) {
        int control2Status = doc["control_2"];
        st_mist = (control2Status == 1);
        digitalWrite(control_2_pin, st_mist ? HIGH : LOW);
      }
      if (doc.containsKey("control_3")) {
        int control3Status = doc["control_3"];
        st_heater = (control3Status == 1);
        digitalWrite(control_3_pin, st_heater ? HIGH : LOW);
      }
      if (doc.containsKey("control_4")) {
        int control4Status = doc["control_4"];
        st_mode = (control4Status == 1);
        digitalWrite(control_4_pin, st_mode ? HIGH : LOW);
      }
      
      // Tampilkan Kondisi (Alert dari AI) di baris pertama
      if (doc.containsKey("alert")) {
        String alertStatus = doc["alert"].as<String>();
        lcd.setCursor(0, 0);
        lcd.print("Sts: ");
        lcd.print(alertStatus);
        // Hapus sisa karakter agar tidak menumpuk (sampai sebelum 'OK')
        for(int i = 5 + alertStatus.length(); i < 18; i++) {
            lcd.print(" ");
        }
      }

      // Update tampilan LCD
      updateLCDStatus();

      break;
  }
}

void setup() {
  Serial.begin(115200);
  
  pinMode(control_1_pin, OUTPUT);
  pinMode(control_2_pin, OUTPUT);
  pinMode(control_3_pin, OUTPUT);
  pinMode(control_4_pin, OUTPUT);
  
  // Matikan semua komponen saat awal nyala
  digitalWrite(control_1_pin, LOW);
  digitalWrite(control_2_pin, LOW);
  digitalWrite(control_3_pin, LOW);
  digitalWrite(control_4_pin, LOW);
  
  // I2C dan LCD Setup
  Wire.begin(I2C_SDA, I2C_SCL);
  lcd.init(); 
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("IoT LocalStack");
  lcd.setCursor(0, 1);
  lcd.print("Connecting WiFi...");

  // WiFi Setup
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Membersihkan baris 1
  lcd.setCursor(0, 1);
  lcd.print("                    ");
  lcd.setCursor(0, 1);
  lcd.print("IP:");
  lcd.print(WiFi.localIP());

  dht.begin();
  
  // WebSocket Setup
  webSocket.begin(server_ip, server_port, "/ws/sensor");
  webSocket.onEvent(webSocketEvent);
  webSocket.setReconnectInterval(5000);
  
  updateLCDStatus();
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
    
    // Tampilkan Suhu dan Kelembapan di Baris 1 LCD
    lcd.setCursor(0, 1);
    lcd.print("                    "); // Clear the line first
    lcd.setCursor(0, 1);
    lcd.print("T:");
    lcd.print(t, 1); // 1 desimal
    lcd.print("C  H:");
    lcd.print(h, 1); // 1 desimal
    lcd.print("%");
  }
}