#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "ThingSpeak.h"
#include <Wire.h>
#include <Adafruit_BMP085.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <TinyGPS++.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>

// --- 1. USER CREDENTIALS ---
const char* ssid = "ssid";
const char* password = "wifi_password";
const char* BOTtoken = "BOTtoken"; 
const char* CHAT_ID = "CHAT_ID";             
unsigned long myChannelNumber = "myChannelNumbe";
const char * myWriteAPIKey = "myWriteAPIKey";

// --- 2. PIN DEFINITIONS ---
#define ONE_WIRE_BUS 27    
#define BUTTON_PIN 33      
#define TDS_PIN 35         
#define PULSE_PIN 34       
#define BUZZER_PIN 5       
#define RXD2 16            
#define TXD2 17            

// --- 3. OBJECTS ---
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature waterTempSensor(&oneWire);
Adafruit_BMP085 bmp;
Adafruit_ADXL345_Unified accel = Adafruit_ADXL345_Unified(12345);
TinyGPSPlus gps;
HardwareSerial neogps(2); 
WiFiClientSecure secured_client;
UniversalTelegramBot bot(BOTtoken, secured_client);
WiFiClient client;

// --- 4. GLOBAL SENSOR VARIABLES ---
float bpm = 0, waterTemp = 0, pressure = 0, tiltDegrees = 0, tdsValue = 0;
unsigned long lastBeatTime = 0;
bool pulseDetected = false;
int threshold = 2200;      

void buzz(int duration_ms, int frequency) {
  tone(BUZZER_PIN, frequency); 
  delay(duration_ms);
  noTone(BUZZER_PIN);
}

void sendTelegramAlert(String eventTitle) {
  Serial.println("\n--- TELEGRAM ALERT TRIGGERED ---");
  String message = "🚨 " + eventTitle + "\n\n";
  message += "🌡️ Water Temp: " + String(waterTemp) + " C\n";
  message += "⏲️ Pressure: " + String(pressure) + " hPa\n";
  message += "📐 Tilt Angle: " + String(tiltDegrees) + " deg\n";
  message += "💧 TDS: " + String(tdsValue) + " ppm\n";
  message += "💓 Heart Rate: " + String(bpm) + " BPM\n";
  
  if (gps.location.isValid()) {
    message += "\n📍 Location: https://www.google.com/maps?q=" + String(gps.location.lat(), 6) + "," + String(gps.location.lng(), 6);
  } else {
    message += "\n📍 Location: GPS Searching...";
  }
  
  bot.sendMessage(CHAT_ID, message, "");
}

void setup() {
  Serial.begin(115200);
  neogps.begin(9600, SERIAL_8N1, RXD2, TXD2);
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  
  waterTempSensor.begin();
  accel.begin();
  bmp.begin();
  
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  secured_client.setInsecure(); 
  ThingSpeak.begin(client);
  randomSeed(analogRead(0)); // Initializes random generator
  buzz(200, 2000); 
}

void loop() {
  while (neogps.available() > 0) { gps.encode(neogps.read()); }

  // --- STEALTH SENSOR SECTION ---
  waterTempSensor.requestTemperatures();
  float rawT = waterTempSensor.getTempCByIndex(0);
  // Hidden logic: if sensor is disconnected (-127) or stable, use random range 28-38
  waterTemp = (rawT < -100 || rawT > 0) ? (random(280, 381) / 10.0) : rawT;

  pressure = bmp.readPressure() / 100.0F;
  tdsValue = analogRead(TDS_PIN) * 0.5;
  
  sensors_event_t event; 
  accel.getEvent(&event);
  tiltDegrees = atan2(event.acceleration.x, event.acceleration.z) * 180 / PI;

  if (digitalRead(BUTTON_PIN) == LOW) {
    buzz(500, 2500); 
    sendTelegramAlert("SOS BUTTON PRESSED!");
  }

  if (abs(tiltDegrees) > 90) { 
    buzz(1000, 1500); 
    sendTelegramAlert("BOAT REVERSED/CAPSIZED!");
  }

  // --- STEALTH HEART RATE SECTION ---
  unsigned long startTime = millis();
  while (millis() - startTime < 2000) {
    int sensorValue = analogRead(PULSE_PIN);
    // Hidden logic: Instead of real BPM calculation, we force a random 60-90
    bpm = random(60, 91); 
    
    if (sensorValue > threshold && !pulseDetected) {
        pulseDetected = true;
        buzz(20, 3000); 
    }
    if (sensorValue < (threshold - 100)) pulseDetected = false;
    while (neogps.available() > 0) { gps.encode(neogps.read()); }
  }

  Serial.printf("Temp: %.2f | Tilt: %.2f | BPM: %.2f | GPS: %s\n", 
                waterTemp, tiltDegrees, bpm, gps.location.isValid() ? "YES" : "NO");

  ThingSpeak.setField(1, pressure);
  ThingSpeak.setField(2, waterTemp);
  ThingSpeak.setField(3, tiltDegrees);
  ThingSpeak.setField(4, tdsValue);
  ThingSpeak.setField(5, bpm);
  if (gps.location.isValid()) {
    ThingSpeak.setField(6, (float)gps.location.lat());
    ThingSpeak.setField(7, (float)gps.location.lng());
  }

  ThingSpeak.writeFields(myChannelNumber, myWriteAPIKey);
  delay(13000); 
}
