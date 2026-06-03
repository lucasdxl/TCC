#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// Pins mapping
#define ONEWIRE_PIN 4
#define PH_PIN 34
#define TURB_PIN 35
#define ORP_PIN 32

// Endpoint
const char* serverUrl = "https://api-monitoramento-agua.onrender.com/leituras";

// WiFi
const char* ssid = "Wokwi-GUEST";
const char* password = "";

// OneWire and temperature sensor
OneWire oneWire(ONEWIRE_PIN);
DallasTemperature sensors(&oneWire);

unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 5000;

// ============ SETUP MINIMO PARA TESTE ============
void setup() {
  // Inicializar Serial ABSOLUTAMENTE no início
  Serial.begin(115200);
  delay(1000);
  Serial.flush();
  
  // Primeira mensagem
  Serial.println("\n\n================================");
  Serial.println("BOOT ESP32 WOKWI");
  Serial.println("================================\n");
  Serial.flush();
}

// ============ LOOP MINIMO PARA TESTE ============
void loop() {
  static unsigned long lastPrint = 0;
  
  // Imprimir "LOOP OK" a cada 1 segundo
  if (millis() - lastPrint >= 1000) {
    lastPrint = millis();
    Serial.println("LOOP OK - Tempo: " + String(millis() / 1000) + "s");
    Serial.flush();
  }
  
  delay(100);
}
