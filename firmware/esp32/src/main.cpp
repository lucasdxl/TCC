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

void connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return;

  Serial.print("Conectando ao Wi-Fi: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print('.');
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWi-Fi conectado");
    Serial.print("IP local: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nFalha ao conectar Wi-Fi");
  }
}

String getTimestamp() {
  time_t nowSec = time(nullptr);
  struct tm timeinfo;
  localtime_r(&nowSec, &timeinfo);
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

float mapAnalogToRange(int pin, float minVal, float maxVal) {
  int raw = analogRead(pin);
  float normalized = (float)raw / 4095.0;
  return minVal + normalized * (maxVal - minVal);
}

void setup() {
  Serial.begin(115200);
  delay(100);

  analogSetPinAttenuation(PH_PIN, ADC_11db);
  analogSetPinAttenuation(TURB_PIN, ADC_11db);
  analogSetPinAttenuation(ORP_PIN, ADC_11db);

  sensors.begin();

  connectWiFi();

  configTime(-3 * 3600, 0, "pool.ntp.org");
  Serial.println("Sincronizando horario via NTP...");
  time_t nowSec = time(nullptr);
  int attempts = 0;
  while (nowSec < 1000000000 && attempts < 20) {
    delay(500);
    nowSec = time(nullptr);
    attempts++;
  }

  Serial.println("Horario inicializado: " + getTimestamp());
}

void sendReading() {
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("Erro: DS18B20 desconectado");
    temperature = 0.0;
  }

  float ph = mapAnalogToRange(PH_PIN, 6.5, 8.5);
  float turbidez = mapAnalogToRange(TURB_PIN, 1.0, 50.0);
  float orp = mapAnalogToRange(ORP_PIN, 200.0, 300.0);
  String timestamp = getTimestamp();

  StaticJsonDocument<256> doc;
  doc["ph"] = ph;
  doc["turbidez"] = turbidez;
  doc["temperatura"] = temperature;
  doc["orp"] = orp;
  doc["data_hora"] = timestamp;

  String payload;
  serializeJson(doc, payload);

  Serial.println("\n--- Enviando leitura ---");
  Serial.print("Wi-Fi status: ");
  Serial.println(WiFi.status());
  Serial.print("IP local: ");
  Serial.println(WiFi.localIP());
  Serial.print("Valores lidos: pH=");
  Serial.print(ph, 2);
  Serial.print(" turbidez=");
  Serial.print(turbidez, 2);
  Serial.print(" ORP=");
  Serial.print(orp, 2);
  Serial.print(" temperatura=");
  Serial.println(temperature, 2);
  Serial.println("JSON: " + payload);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient https;
  if (https.begin(client, serverUrl)) {
    https.addHeader("Content-Type", "application/json");
    int httpCode = https.POST(payload);
    if (httpCode <= 0) {
      Serial.println("Falha HTTP, codigo: " + String(httpCode));
    } else {
      Serial.println("HTTP code: " + String(httpCode));
      String response = https.getString();
      Serial.println("Resposta API: " + response);
    }
    https.end();
  } else {
    Serial.println("Falha ao iniciar conexao HTTPS");
  }
  Serial.println("------------------------\n");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("Wi-Fi desconectado, tentando reconectar...");
    connectWiFi();
  }

  if (millis() - lastSend >= SEND_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      sendReading();
    } else {
      Serial.println("Nao ha conexao Wi-Fi, leitura nao enviada.");
    }
    lastSend = millis();
  }

  delay(10);
}
