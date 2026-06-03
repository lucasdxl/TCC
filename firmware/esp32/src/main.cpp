#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>

// Pins mapping
#define ONEWIRE_PIN 14
#define PH_PIN 34
#define TURB_PIN 35
#define ORP_PIN 32

// Endpoint (HTTPS com WiFiClientSecure)
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

  Serial.print("\n[WiFi] Conectando ao: ");
  Serial.println(ssid);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 10000) {
    Serial.print('.');
    delay(500);
  }

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] CONECTADO!");
    Serial.print("[WiFi] IP local: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n[WiFi] ERRO ao conectar");
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

void sendReading() {
  // Verificar se a hora foi sincronizada
  time_t nowSec = time(nullptr);
  if (nowSec < 1000000000) {
    Serial.println("[SENSOR] ERRO: Hora nao sincronizada. Pulando envio.");
    return;
  }

  // Leitura dos sensores
  sensors.requestTemperatures();
  float temperature = sensors.getTempCByIndex(0);
  if (temperature == DEVICE_DISCONNECTED_C) {
    Serial.println("[SENSOR] ERRO: DS18B20 desconectado");
    temperature = 0.0;
  }

  float ph = mapAnalogToRange(PH_PIN, 6.5, 8.5);
  float turbidez = mapAnalogToRange(TURB_PIN, 1.0, 50.0);
  float orp = mapAnalogToRange(ORP_PIN, 200.0, 300.0);
  String timestamp = getTimestamp();

  // Criar JSON no formato exato
  StaticJsonDocument<256> doc;
  doc["ph"] = ph;
  doc["turbidez"] = turbidez;
  doc["temperatura"] = temperature;
  doc["orp"] = orp;
  doc["data_hora"] = timestamp;

  String payload;
  serializeJson(doc, payload);

  // ===== DEBUG: Imprimir dados =====
  Serial.println("\n[HTTP] ========== ENVIANDO LEITURA ==========");
  Serial.print("[URL] ");
  Serial.println(serverUrl);
  
  Serial.print("[JSON] ");
  Serial.println(payload);

  // ===== Envio HTTPS com SSL =====
  WiFiClientSecure client;
  client.setInsecure();  // Desabilitar verificacao de certificado para Wokwi
  
  HTTPClient http;
  
  Serial.println("[HTTP] Iniciando conexao HTTPS...");
  
  if (http.begin(client, serverUrl)) {
    Serial.println("[HTTP] Conexao HTTPS iniciada!");
    
    // Header
    http.addHeader("Content-Type", "application/json");
    
    // POST
    Serial.println("[HTTP] Enviando POST...");
    int httpCode = http.POST(payload);
    
    Serial.print("[HTTP] Codigo HTTP: ");
    Serial.println(httpCode);
    
    // Imprimir mensagem de erro se houver
    if (httpCode < 0) {
      Serial.print("[HTTP] ERRO DE CONEXAO: ");
      Serial.println(http.errorToString(httpCode));
    }
    
    // Imprimir resposta completa
    String response = http.getString();
    Serial.print("[RESPOSTA] ");
    Serial.println(response);
    
    // Considerar sucesso apenas 200 ou 201 (nao 307)
    if (httpCode == 201 || httpCode == 200) {
      Serial.println("[HTTP] ✓ SUCESSO! Leitura registrada.");
    } else {
      Serial.print("[HTTP] ✗ FALHA! Esperado 200/201, recebido: ");
      Serial.println(httpCode);
    }
    
    http.end();
  } else {
    Serial.println("[HTTP] ✗ ERRO ao iniciar conexao HTTPS!");
  }
  
  Serial.println("[HTTP] ========== FIM DO ENVIO ==========\n");
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.flush();
  
  Serial.println("\n\n================================");
  Serial.println("BOOT ESP32 WOKWI");
  Serial.println("================================\n");

  Serial.println("[SETUP] Inicializando GPIO e sensores...");
  analogSetPinAttenuation(PH_PIN, ADC_11db);
  analogSetPinAttenuation(TURB_PIN, ADC_11db);
  analogSetPinAttenuation(ORP_PIN, ADC_11db);

  sensors.begin();
  Serial.println("[SETUP] Sensores inicializados.\n");

  Serial.println("[SETUP] Conectando ao Wi-Fi...");
  connectWiFi();

  Serial.println("\n[SETUP] Sincronizando horario via NTP...");
  configTime(-3 * 3600, 0, "pool.ntp.org");
  time_t nowSec = time(nullptr);
  int attempts = 0;
  while (nowSec < 1000000000 && attempts < 20) {
    delay(500);
    nowSec = time(nullptr);
    attempts++;
  }

  Serial.println("[SETUP] Horario inicializado: " + getTimestamp());
  Serial.println("[SETUP] Setup completo! Iniciando loop...\n");
}

void loop() {
  static unsigned long lastDebugTime = 0;
  
  // Debug a cada 10 segundos
  if (millis() - lastDebugTime >= 10000) {
    lastDebugTime = millis();
    Serial.print("[LOOP] Rodando... ");
    Serial.print(millis() / 1000);
    Serial.print("s | WiFi: ");
    Serial.println(WiFi.status() == WL_CONNECTED ? "OK" : "ERRO");
  }

  // Reconectar Wi-Fi se necessário
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[LOOP] Wi-Fi desconectado, tentando reconectar...");
    connectWiFi();
  }

  // Enviar leitura a cada SEND_INTERVAL
  if (millis() - lastSend >= SEND_INTERVAL) {
    if (WiFi.status() == WL_CONNECTED) {
      sendReading();
    } else {
      Serial.println("[LOOP] Nao ha conexao Wi-Fi. Pulando envio.");
    }
    lastSend = millis();
  }

  delay(100);
}
