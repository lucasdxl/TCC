#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <time.h>
#include <math.h>
#include <float.h>

// =====================================================
// PINOS
// =====================================================

constexpr uint8_t ONEWIRE_PIN = 14;
constexpr uint8_t PH_PIN = 34;
constexpr uint8_t TURB_PIN = 35;
constexpr uint8_t ORP_PIN = 32;

// =====================================================
// CONFIGURAÇÃO DE OPERAÇÃO
// =====================================================

// true  = banca / desenvolvimento
// false = cenário real
constexpr bool MODO_DEMONSTRACAO = true;

// Intervalo APÓS o término de um ciclo completo
constexpr unsigned long INTERVALO_ENTRE_CICLOS =
    MODO_DEMONSTRACAO
        ? 10000UL       // 10 segundos
        : 300000UL;     // 5 minutos

// =====================================================
// AMOSTRAGEM
// =====================================================

// pH, turbidez e ORP:
// 10 amostras, uma por segundo
constexpr uint8_t NUM_AMOSTRAS_ANALOGICAS = 10;
constexpr unsigned long INTERVALO_AMOSTRAS_ANALOGICAS = 500UL;

// Temperatura:
// 3 amostras por ciclo
constexpr uint8_t NUM_AMOSTRAS_TEMPERATURA = 3;

// Pequeno intervalo extra entre medições
constexpr unsigned long INTERVALO_EXTRA_TEMPERATURA = 50UL;

// =====================================================
// API
// =====================================================

const char* SERVER_URL =
    "https://api-monitoramento-agua.onrender.com/leituras";

// =====================================================
// WI-FI
// =====================================================

const char* WIFI_SSID = "Wokwi-GUEST";
const char* WIFI_PASSWORD = "";

constexpr unsigned long WIFI_TIMEOUT_MS = 10000UL;

// =====================================================
// HTTPS
// =====================================================

constexpr uint8_t MAX_HTTP_ATTEMPTS = 3;
constexpr unsigned long HTTP_RETRY_DELAY_MS = 5000UL;
constexpr unsigned long HTTP_TIMEOUT_MS = 15000UL;

// =====================================================
// SENSOR DE TEMPERATURA
// =====================================================

OneWire oneWire(ONEWIRE_PIN);
DallasTemperature temperatureSensor(&oneWire);

// =====================================================
// ESTATÍSTICAS
// =====================================================

struct Estatisticas {
  float media = 0.0f;
  float minimo = 0.0f;
  float maximo = 0.0f;
  float desvioPadrao = 0.0f;
  uint16_t quantidade = 0;
};

struct Acumulador {
  float soma = 0.0f;
  float somaQuadrados = 0.0f;
  float minimo = FLT_MAX;
  float maximo = -FLT_MAX;
  uint16_t quantidade = 0;
};

// =====================================================
// CONTROLE DO CICLO
// =====================================================

unsigned long ultimoCicloConcluido = 0;

// =====================================================
// FUNÇÕES ESTATÍSTICAS
// =====================================================

void adicionarAmostra(Acumulador& acumulador, float valor) {
  acumulador.soma += valor;
  acumulador.somaQuadrados += valor * valor;

  acumulador.minimo = min(acumulador.minimo, valor);
  acumulador.maximo = max(acumulador.maximo, valor);

  acumulador.quantidade++;
}

Estatisticas calcularEstatisticas(const Acumulador& acumulador) {
  Estatisticas resultado;

  if (acumulador.quantidade == 0) {
    return resultado;
  }

  const float n = static_cast<float>(acumulador.quantidade);

  resultado.media = acumulador.soma / n;
  resultado.minimo = acumulador.minimo;
  resultado.maximo = acumulador.maximo;
  resultado.quantidade = acumulador.quantidade;

  float variancia =
      (acumulador.somaQuadrados / n) -
      (resultado.media * resultado.media);

  if (variancia < 0.0f) {
    variancia = 0.0f;
  }

  resultado.desvioPadrao = sqrtf(variancia);

  return resultado;
}

// =====================================================
// WI-FI
// =====================================================

bool conectarWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return true;
  }

  Serial.println();
  Serial.print("[WiFi] Conectando a ");
  Serial.println(WIFI_SSID);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  const unsigned long inicio = millis();

  while (
      WiFi.status() != WL_CONNECTED &&
      millis() - inicio < WIFI_TIMEOUT_MS
  ) {
    Serial.print(".");
    delay(500);
  }

  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Falha na conexao.");
    return false;
  }

  Serial.println("[WiFi] Conectado.");
  Serial.print("[WiFi] IP: ");
  Serial.println(WiFi.localIP());

  return true;
}

// =====================================================
// NTP / DATA E HORA
// =====================================================

bool horarioSincronizado() {
  return time(nullptr) >= 1000000000;
}

String obterTimestamp() {
  time_t agora = time(nullptr);

  struct tm dadosTempo;
  localtime_r(&agora, &dadosTempo);

  char buffer[20];

  strftime(
      buffer,
      sizeof(buffer),
      "%Y-%m-%d %H:%M:%S",
      &dadosTempo
  );

  return String(buffer);
}

void inicializarNTP() {
  Serial.println();
  Serial.println("[NTP] Sincronizando horario...");

  configTime(
      -3 * 3600,
      0,
      "pool.ntp.org"
  );

  constexpr uint8_t MAX_NTP_ATTEMPTS = 20;

  for (
      uint8_t tentativa = 0;
      tentativa < MAX_NTP_ATTEMPTS;
      tentativa++
  ) {
    if (horarioSincronizado()) {
      Serial.print("[NTP] Horario sincronizado: ");
      Serial.println(obterTimestamp());
      return;
    }

    delay(500);
  }

  Serial.println("[NTP] Nao foi possivel sincronizar.");
  Serial.println("[NTP] A API utilizara o horario do servidor.");
}

// =====================================================
// CONVERSÃO ADC
// =====================================================

float converterADC(
    int valorRaw,
    float valorMinimo,
    float valorMaximo
) {
  const float normalizado =
      static_cast<float>(valorRaw) / 4095.0f;

  return valorMinimo +
         normalizado * (valorMaximo - valorMinimo);
}

// =====================================================
// AMOSTRAGEM ANALÓGICA
// =====================================================

void lerSensoresAnalogicos(
    Estatisticas& ph,
    Estatisticas& turbidez,
    Estatisticas& orp
) {
  Acumulador acumuladorPh;
  Acumulador acumuladorTurbidez;
  Acumulador acumuladorOrp;

  Serial.println();
  Serial.println("[SENSOR] Amostragem analogica iniciada");

  for (
      uint8_t amostra = 1;
      amostra <= NUM_AMOSTRAS_ANALOGICAS;
      amostra++
  ) {
    const float valorPh =
        converterADC(
            analogRead(PH_PIN),
            6.5f,
            8.5f
        );

    const float valorTurbidez =
        converterADC(
            analogRead(TURB_PIN),
            1.0f,
            50.0f
        );

    const float valorOrp =
        converterADC(
            analogRead(ORP_PIN),
            200.0f,
            300.0f
        );

    adicionarAmostra(acumuladorPh, valorPh);
    adicionarAmostra(acumuladorTurbidez, valorTurbidez);
    adicionarAmostra(acumuladorOrp, valorOrp);

    Serial.print("[AMOSTRA ");
    Serial.print(amostra);
    Serial.print("/");
    Serial.print(NUM_AMOSTRAS_ANALOGICAS);
    Serial.print("] pH=");
    Serial.print(valorPh, 2);
    Serial.print(" | Turbidez=");
    Serial.print(valorTurbidez, 2);
    Serial.print(" | ORP=");
    Serial.println(valorOrp, 2);

    if (amostra < NUM_AMOSTRAS_ANALOGICAS) {
      delay(INTERVALO_AMOSTRAS_ANALOGICAS);
    }
  }

  ph = calcularEstatisticas(acumuladorPh);
  turbidez = calcularEstatisticas(acumuladorTurbidez);
  orp = calcularEstatisticas(acumuladorOrp);
}

// =====================================================
// AMOSTRAGEM DE TEMPERATURA
// =====================================================

Estatisticas lerTemperatura() {
  Acumulador acumulador;

  Serial.println();
  Serial.println("[SENSOR] Amostragem de temperatura iniciada");

  for (
      uint8_t amostra = 1;
      amostra <= NUM_AMOSTRAS_TEMPERATURA;
      amostra++
  ) {
    temperatureSensor.requestTemperatures();

    const float valor =
        temperatureSensor.getTempCByIndex(0);

    if (valor == DEVICE_DISCONNECTED_C) {
      Serial.print("[TEMP ");
      Serial.print(amostra);
      Serial.print("/");
      Serial.print(NUM_AMOSTRAS_TEMPERATURA);
      Serial.println("] Sensor desconectado");

      continue;
    }

    adicionarAmostra(acumulador, valor);

    Serial.print("[TEMP ");
    Serial.print(amostra);
    Serial.print("/");
    Serial.print(NUM_AMOSTRAS_TEMPERATURA);
    Serial.print("] ");
    Serial.print(valor, 2);
    Serial.println(" C");

    if (amostra < NUM_AMOSTRAS_TEMPERATURA) {
      delay(INTERVALO_EXTRA_TEMPERATURA);
    }
  }

  return calcularEstatisticas(acumulador);
}

// =====================================================
// EXIBIÇÃO DOS RESULTADOS
// =====================================================

void imprimirEstatistica(
    const char* nome,
    const Estatisticas& dados,
    const char* unidade = ""
) {
  Serial.print(nome);
  Serial.print(" -> Media: ");
  Serial.print(dados.media, 2);
  Serial.print(unidade);

  Serial.print(" | Min: ");
  Serial.print(dados.minimo, 2);
  Serial.print(unidade);

  Serial.print(" | Max: ");
  Serial.print(dados.maximo, 2);
  Serial.print(unidade);

  Serial.print(" | Desvio: ");
  Serial.print(dados.desvioPadrao, 3);

  Serial.print(" | N: ");
  Serial.println(dados.quantidade);
}

void imprimirResultados(
    const Estatisticas& ph,
    const Estatisticas& turbidez,
    const Estatisticas& temperatura,
    const Estatisticas& orp
) {
  Serial.println();
  Serial.println("===== RESULTADO CONSOLIDADO =====");

  imprimirEstatistica("pH", ph);
  imprimirEstatistica("Turbidez", turbidez, " NTU");
  imprimirEstatistica("ORP", orp, " mV");
  imprimirEstatistica("Temperatura", temperatura, " C");
}

// =====================================================
// HTTPS
// =====================================================

int enviarPostComRetry(const String& payload) {
  for (
      uint8_t tentativa = 1;
      tentativa <= MAX_HTTP_ATTEMPTS;
      tentativa++
  ) {
    Serial.print("[HTTP] Tentativa ");
    Serial.print(tentativa);
    Serial.print("/");
    Serial.println(MAX_HTTP_ATTEMPTS);

    if (!conectarWiFi()) {
      Serial.println("[HTTP] Sem Wi-Fi. Tentativa cancelada.");
    } else {
      WiFiClientSecure client;

      client.setInsecure();
      client.setTimeout(HTTP_TIMEOUT_MS);
      client.setHandshakeTimeout(15);

      HTTPClient http;

      http.setTimeout(HTTP_TIMEOUT_MS);
      http.setReuse(false);

      if (!http.begin(client, SERVER_URL)) {
        Serial.println("[HTTP] Falha ao iniciar conexao HTTPS.");

        client.stop();
      } else {
        http.addHeader(
            "Content-Type",
            "application/json"
        );

        Serial.println("[HTTP] Enviando POST...");

        const unsigned long inicio = millis();
        const int codigo = http.POST(payload);
        const unsigned long duracao = millis() - inicio;

        Serial.print("[HTTP] Tempo: ");
        Serial.print(duracao);
        Serial.print(" ms | Codigo: ");
        Serial.println(codigo);

        if (codigo > 0) {
          Serial.print("[API] ");
          Serial.println(http.getString());
        } else {
          Serial.print("[HTTP] Erro: ");
          Serial.println(http.errorToString(codigo));
        }

        http.end();
        client.stop();

        if (
            codigo == HTTP_CODE_OK ||
            codigo == HTTP_CODE_CREATED
        ) {
          Serial.println("[HTTP] Leitura registrada com sucesso.");

          return codigo;
        }
      }
    }

    if (tentativa < MAX_HTTP_ATTEMPTS) {
      Serial.print("[HTTP] Nova tentativa em ");
      Serial.print(HTTP_RETRY_DELAY_MS);
      Serial.println(" ms...");

      delay(HTTP_RETRY_DELAY_MS);
    }
  }

  Serial.println("[HTTP] Todas as tentativas falharam.");

  return -1;
}

// =====================================================
// JSON
// =====================================================

String criarPayload(
    const Estatisticas& ph,
    const Estatisticas& turbidez,
    const Estatisticas& temperatura,
    const Estatisticas& orp
) {
  StaticJsonDocument<256> documento;

  // Por enquanto a API recebe apenas os valores médios.
  documento["ph"] = ph.media;
  documento["turbidez"] = turbidez.media;
  documento["temperatura"] = temperatura.media;
  documento["orp"] = orp.media;

  if (horarioSincronizado()) {
    documento["data_hora"] = obterTimestamp();
  } else {
    Serial.println("[NTP] Payload enviado sem data_hora.");
  }

  String payload;
  serializeJson(documento, payload);

  return payload;
}

// =====================================================
// CICLO COMPLETO
// =====================================================

void executarCicloMedicao() {
  Serial.println();
  Serial.println("========================================");
  Serial.println("NOVO CICLO DE MEDICAO");
  Serial.println("========================================");

  Estatisticas ph;
  Estatisticas turbidez;
  Estatisticas orp;

  lerSensoresAnalogicos(
      ph,
      turbidez,
      orp
  );

  const Estatisticas temperatura =
      lerTemperatura();

  imprimirResultados(
      ph,
      turbidez,
      temperatura,
      orp
  );

  const String payload =
      criarPayload(
          ph,
          turbidez,
          temperatura,
          orp
      );

  Serial.println();
  Serial.println("===== ENVIO DA LEITURA CONSOLIDADA =====");

  Serial.print("[JSON] ");
  Serial.println(payload);

  const int resultado =
      enviarPostComRetry(payload);

  Serial.println();

  if (resultado < 0) {
    Serial.println("[CICLO] Concluido sem envio.");
  } else {
    Serial.println("[CICLO] Concluido com sucesso.");
  }

  Serial.print("[PROXIMO] Novo ciclo em ");
  Serial.print(INTERVALO_ENTRE_CICLOS / 1000);
  Serial.println(" segundos.");

  Serial.println("========================================");
}

// =====================================================
// SETUP
// =====================================================

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("========================================");
  Serial.println("MONITORAMENTO DE QUALIDADE DA AGUA");
  Serial.println("========================================");

  Serial.print("[MODO] ");
  Serial.println(
      MODO_DEMONSTRACAO
          ? "DEMONSTRACAO"
          : "OPERACAO REAL"
  );

  Serial.print("[MODO] Intervalo entre ciclos: ");
  Serial.print(INTERVALO_ENTRE_CICLOS / 1000);
  Serial.println(" segundos");

  analogSetPinAttenuation(
      PH_PIN,
      ADC_11db
  );

  analogSetPinAttenuation(
      TURB_PIN,
      ADC_11db
  );

  analogSetPinAttenuation(
      ORP_PIN,
      ADC_11db
  );

  temperatureSensor.begin();

  if (!conectarWiFi()) {
    Serial.println("[SETUP] Wi-Fi indisponivel.");
  }

  inicializarNTP();

  ultimoCicloConcluido = millis();

  Serial.println();
  Serial.println("[SETUP] Sistema pronto.");
  Serial.print("[SETUP] Primeiro ciclo em ");
  Serial.print(INTERVALO_ENTRE_CICLOS / 1000);
  Serial.println(" segundos.");
}

// =====================================================
// LOOP
// =====================================================

void loop() {
  // Reconecta apenas quando realmente há perda de Wi-Fi.
  if (WiFi.status() != WL_CONNECTED) {
    conectarWiFi();
  }

  const bool horaDoProximoCiclo =
      millis() - ultimoCicloConcluido >=
      INTERVALO_ENTRE_CICLOS;

  if (!horaDoProximoCiclo) {
    delay(100);
    return;
  }

  executarCicloMedicao();

  // O intervalo começa somente após o ciclo terminar.
  ultimoCicloConcluido = millis();
}