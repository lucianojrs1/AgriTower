#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "time.h"

// =========================================================================
// 1. CONFIGURAÇÕES DE HARDWARE, WI-FI, THINGSBOARD E NTP
// =========================================================================

// --- Pinos e configuração da UART2 com a BitDogLab ---
#define RXp2      16       // RX da ESP32 <- TX da BitDogLab (GP8)
#define TXp2      17       // TX da ESP32 -> RX da BitDogLab (GP9)
#define BAUD_PICO 9600     // Mesmo baud rate do relogio.cpp / comunicacao.cpp

// --- LEDs de Status ---
#define LED_WIFI   25      // Aceso fixo = conectado | Piscando = reconectando
#define LED_SERIAL 26      // Pisca ao receber pacote válido da BitDogLab
const unsigned long TEMPO_LED_SERIAL = 100;

// --- Credenciais Wi-Fi e Endpoint ThingsBoard ---
const char* ssid   = "CINGUESTS";
const char* senha  = "acessocin";
const char* server = "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/telemetry";
const char* urlAtributos = "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/attributes?sharedKeys=data_plantio";

// --- Configuração NTP (Fuso Recife/Brasília GMT-3) ---
const char* ntpServer          = "pool.ntp.org";
const long  gmtOffset_sec      = -10800;
const int   daylightOffset_sec = 0;

// --- Temporizadores de Controle (Não-Bloqueantes) ---
unsigned long ledSerialDesligaEm = 0;
unsigned long ultimaPiscadaWifi  = 0;
bool          estadoLedWifi      = false;

unsigned long ultimoEnvioHora          = 0;
const unsigned long intervaloEnvioHora       = 60000; // Envia "T:HH:MM" a cada 60 s
const unsigned long intervaloRetentativaHora = 5000;  // Retenta a cada 5 s enquanto sem NTP
bool          horaEnviada              = false;

unsigned long ultimaSincronizacaoAtributos = 0;
const unsigned long intervaloAtributos     = 5000;    // Consulta data_plantio a cada 5 s

unsigned long ultimoPostTelemetria     = 0;
const unsigned long intervaloPostTb    = 2000;        // Envia POST HTTP ao ThingsBoard a cada 2 s

// =========================================================================
// 2. MODELO DE ÁRVORE DE DECISÃO E REFERÊNCIA DO DATASET (ALFACE)
// =========================================================================
// Entradas: x[0]=Temperatura (°C), x[1]=Umidade (%), x[2]=TDS (ppm),
//           x[3]=pH, x[4]=Dias de crescimento
// Saída:    0 = Crescendo, 1 = Pronto para colher, 2 = Estragado
namespace Eloquent {
  namespace ML {
    namespace Port {
      class DecisionTree {
        public:
          float confianca = 0;

          int predict(float *x) {
            if (x[4] <= 34.500000f) {
                if (x[0] <= 32.850000f) {
                    confianca = 1.0000f;
                    return 0;
                } else {
                    if (x[3] <= 6.150000f) {
                        confianca = 1.0000f;
                        return 2;
                    } else {
                        if (x[3] <= 6.650000f) {
                            confianca = 1.0000f;
                            return 0;
                        } else {
                            confianca = 1.0000f;
                            return 2;
                        }
                    }
                }
            } else {
                if (x[4] <= 42.500000f) {
                    confianca = 1.0000f;
                    return 1;
                } else {
                    confianca = 1.0000f;
                    return 2;
                }
            }
          }
      };
    }
  }
}

Eloquent::ML::Port::DecisionTree modeloAlface;

const int NUM_SENSORES_REF = 4;
const char* NOMES_SENSORES_REF[NUM_SENSORES_REF] = {"Temperatura", "Umidade", "TDS", "pH"};
const char* UNIDADES_REF[NUM_SENSORES_REF]       = {" °C", " %", " ppm", ""};

const float REF_MIN[NUM_SENSORES_REF] = {18.00f, 50.00f, 400.00f, 6.00f};
const float REF_MAX[NUM_SENSORES_REF] = {33.50f, 80.00f, 800.00f, 6.80f};
const float REF_P5[NUM_SENSORES_REF]  = {19.10f, 51.00f, 418.00f, 6.00f};
const float REF_P95[NUM_SENSORES_REF] = {33.40f, 79.00f, 781.35f, 6.80f};

const int REF_DIA_MIN = 1;
const int REF_DIA_MAX = 48;

const float REF_MEDIA_DIA[48][NUM_SENSORES_REF] = {
  {30.07f, 64.90f, 602.59f, 6.39f}, {30.07f, 64.69f, 602.92f, 6.38f},
  {30.10f, 64.65f, 603.22f, 6.38f}, {29.97f, 64.91f, 599.79f, 6.38f},
  {29.78f, 65.23f, 597.47f, 6.37f}, {28.28f, 65.37f, 602.27f, 6.38f},
  {28.04f, 66.00f, 602.23f, 6.39f}, {27.58f, 66.13f, 600.21f, 6.41f},
  {27.25f, 66.23f, 600.99f, 6.41f}, {26.97f, 65.83f, 604.68f, 6.41f},
  {28.02f, 65.43f, 599.59f, 6.41f}, {27.71f, 64.55f, 600.97f, 6.40f},
  {27.62f, 64.54f, 597.36f, 6.40f}, {27.75f, 64.29f, 600.88f, 6.41f},
  {27.80f, 64.39f, 595.17f, 6.41f}, {27.95f, 64.47f, 596.74f, 6.42f},
  {28.08f, 64.97f, 595.47f, 6.42f}, {28.22f, 64.61f, 605.01f, 6.41f},
  {28.22f, 64.72f, 601.95f, 6.40f}, {28.32f, 64.70f, 601.52f, 6.39f},
  {28.40f, 64.74f, 599.30f, 6.40f}, {28.55f, 64.47f, 602.60f, 6.40f},
  {28.47f, 65.01f, 597.64f, 6.41f}, {28.20f, 64.85f, 599.51f, 6.40f},
  {27.79f, 64.83f, 605.39f, 6.40f}, {27.47f, 64.91f, 604.49f, 6.40f},
  {27.12f, 64.77f, 600.49f, 6.40f}, {27.39f, 63.97f, 602.62f, 6.41f},
  {27.78f, 64.35f, 601.08f, 6.42f}, {28.25f, 64.24f, 595.97f, 6.42f},
  {28.57f, 64.09f, 589.53f, 6.41f}, {29.07f, 64.57f, 590.53f, 6.42f},
  {29.08f, 65.53f, 582.79f, 6.41f}, {29.00f, 65.33f, 582.10f, 6.39f},
  {28.61f, 65.86f, 581.75f, 6.39f}, {26.96f, 65.89f, 585.90f, 6.39f},
  {26.71f, 65.52f, 581.45f, 6.37f}, {26.60f, 65.02f, 588.91f, 6.38f},
  {26.61f, 64.65f, 591.67f, 6.40f}, {26.79f, 64.20f, 593.92f, 6.39f},
  {28.09f, 63.97f, 599.59f, 6.40f}, {28.09f, 63.94f, 603.68f, 6.40f},
  {28.13f, 64.47f, 605.30f, 6.40f}, {28.05f, 64.84f, 603.98f, 6.39f},
  {28.17f, 64.74f, 605.78f, 6.40f}, {28.68f, 65.14f, 607.36f, 6.39f},
  {29.01f, 66.25f, 609.98f, 6.41f}, {29.93f, 63.50f, 585.28f, 6.38f}
};

const float REF_DP_DIA[48][NUM_SENSORES_REF] = {
  {5.63f, 8.89f, 106.84f, 0.24f}, {5.62f, 8.82f, 105.46f, 0.24f},
  {5.57f, 8.98f, 107.33f, 0.24f}, {5.42f, 8.93f, 109.44f, 0.24f},
  {5.33f, 8.85f, 111.06f, 0.23f}, {5.53f, 8.86f, 112.00f, 0.23f},
  {5.37f, 8.87f, 114.25f, 0.23f}, {5.01f, 8.80f, 116.35f, 0.23f},
  {4.77f, 8.87f, 118.32f, 0.23f}, {4.47f, 8.93f, 116.93f, 0.24f},
  {4.21f, 9.04f, 118.74f, 0.24f}, {3.99f, 9.02f, 120.69f, 0.24f},
  {3.96f, 9.02f, 119.65f, 0.24f}, {4.03f, 8.96f, 119.68f, 0.24f},
  {4.15f, 9.13f, 120.17f, 0.24f}, {4.18f, 9.02f, 118.91f, 0.23f},
  {4.10f, 9.01f, 116.24f, 0.23f}, {4.14f, 8.95f, 116.19f, 0.23f},
  {4.10f, 8.89f, 115.96f, 0.23f}, {4.11f, 8.81f, 116.18f, 0.23f},
  {4.16f, 9.05f, 115.63f, 0.23f}, {4.28f, 9.12f, 116.00f, 0.23f},
  {4.28f, 9.14f, 114.47f, 0.23f}, {4.31f, 9.32f, 112.92f, 0.23f},
  {4.18f, 9.10f, 111.69f, 0.23f}, {3.95f, 8.98f, 114.27f, 0.23f},
  {3.78f, 9.02f, 116.06f, 0.23f}, {3.96f, 9.15f, 118.06f, 0.23f},
  {4.12f, 9.01f, 119.14f, 0.22f}, {4.29f, 9.16f, 121.55f, 0.22f},
  {4.56f, 8.88f, 119.85f, 0.23f}, {4.55f, 8.76f, 120.73f, 0.23f},
  {4.63f, 8.54f, 117.99f, 0.24f}, {4.70f, 8.68f, 115.46f, 0.24f},
  {4.75f, 8.76f, 112.01f, 0.24f}, {5.36f, 9.01f, 114.01f, 0.24f},
  {5.36f, 9.26f, 111.77f, 0.24f}, {5.24f, 9.31f, 112.37f, 0.24f},
  {5.14f, 9.32f, 116.60f, 0.24f}, {5.11f, 9.26f, 118.85f, 0.24f},
  {4.30f, 9.21f, 119.22f, 0.24f}, {4.14f, 9.04f, 118.75f, 0.24f},
  {4.15f, 8.95f, 119.63f, 0.24f}, {4.09f, 8.84f, 116.59f, 0.24f},
  {4.04f, 8.76f, 115.47f, 0.24f}, {4.13f, 8.66f, 110.95f, 0.24f},
  {4.19f, 8.45f, 108.07f, 0.23f}, {2.81f, 8.10f, 71.20f, 0.22f}
};

// --- Configuração da Data de Plantio ---
const int PLANTIO_ANO = 2026;
const int PLANTIO_MES = 10;
const int PLANTIO_DIA = 1;
const int DIAS_CRESCIMENTO_MANUAL = -1; // Coloque >= 1 para forçar dias fixos em teste
const int DIAS_SEM_RELOGIO = -9999;

int  plantioAno = PLANTIO_ANO;
int  plantioMes = PLANTIO_MES;
int  plantioDia = PLANTIO_DIA;
bool plantioVeioDoDashboard = false;

const char* NOMES_STATUS_ALFACE[3] = {"Crescendo", "Pronto para colher", "Estragado"};
const char* DICA_ALTA[NUM_SENSORES_REF]  = {
  "ventilar/sombrear a torre",
  "melhorar a ventilação (risco de fungos)",
  "diluir a solução com água",
  "baixar o pH da solução"
};
const char* DICA_BAIXA[NUM_SENSORES_REF] = {
  "proteger a torre do frio",
  "ar seco, a planta perde mais água",
  "repor a solução nutritiva",
  "subir o pH da solução"
};

struct DiagnosticoAlface {
  bool   valido       = false;
  int    status       = -1;
  float  confianca    = 0;
  int    dias         = -1;
  bool   confiavel    = true;
  bool   extrapolando = false;
  int    numAlertas   = 0;
  String conclusao;
};

// =========================================================================
// 3. FUNÇÕES DE CÁLCULO DE CRESCIMENTO E DIAGNÓSTICO DO MODELO
// =========================================================================

int calcularDiasCrescimento() {
  if (DIAS_CRESCIMENTO_MANUAL >= 1) return DIAS_CRESCIMENTO_MANUAL;

  time_t agora = time(nullptr);
  if (agora < 1700000000) return DIAS_SEM_RELOGIO;

  struct tm hoje;
  localtime_r(&agora, &hoje);
  hoje.tm_hour = 12; hoje.tm_min = 0; hoje.tm_sec = 0;

  struct tm plantio = {};
  plantio.tm_year  = plantioAno - 1900;
  plantio.tm_mon   = plantioMes - 1;
  plantio.tm_mday  = plantioDia;
  plantio.tm_hour  = 12;
  plantio.tm_isdst = -1;

  double segundos = difftime(mktime(&hoje), mktime(&plantio));
  return (int)lround(segundos / 86400.0) + 1;
}

bool atualizarDataPlantio(JsonVariantConst valor) {
  int ano = 0, mes = 0, dia = 0;

  if (valor.is<const char*>()) {
    const char* texto = valor.as<const char*>();
    if (sscanf(texto, "%d-%d-%d", &ano, &mes, &dia) != 3 &&
        sscanf(texto, "%d/%d/%d", &dia, &mes, &ano) != 3) {
      ano = 0;
    }
  } else if (valor.is<double>()) {
    time_t segundos = (time_t)(valor.as<double>() / 1000.0);
    struct tm data;
    localtime_r(&segundos, &data);
    ano = data.tm_year + 1900;
    mes = data.tm_mon + 1;
    dia = data.tm_mday;
  }

  if (ano < 2020 || ano > 2100 || mes < 1 || mes > 12 || dia < 1 || dia > 31) {
    static unsigned long ultimoAviso = 0;
    if (ultimoAviso == 0 || millis() - ultimoAviso >= 60000) {
      ultimoAviso = millis();
      Serial.println("[DASHBOARD] data_plantio inválida (use AAAA-MM-DD, DD/MM/AAAA ou timestamp)");
    }
    return false;
  }

  if (!plantioVeioDoDashboard || ano != plantioAno || mes != plantioMes || dia != plantioDia) {
    Serial.printf("[DASHBOARD] Nova data de plantio sincronizada: %02d/%02d/%04d\n", dia, mes, ano);
  }
  plantioAno = ano;
  plantioMes = mes;
  plantioDia = dia;
  plantioVeioDoDashboard = true;
  return true;
}

DiagnosticoAlface analisarAlface(float temperatura, float umidade, float tds, float ph) {
  DiagnosticoAlface d;
  d.dias = calcularDiasCrescimento();

  if (d.dias == DIAS_SEM_RELOGIO) {
    d.conclusao = "Relógio sem NTP: não dá para calcular os dias de crescimento.";
    return d;
  }
  if (d.dias < 1) {
    d.conclusao = "Data de plantio depois de hoje: confira o campo data_plantio no dashboard.";
    return d;
  }
  d.valido = true;

  float x[5] = {temperatura, umidade, tds, ph, (float)d.dias};
  d.status    = modeloAlface.predict(x);
  d.confianca = modeloAlface.confianca;

  d.extrapolando = (d.dias < REF_DIA_MIN || d.dias > REF_DIA_MAX);
  int idxDia = constrain(d.dias, REF_DIA_MIN, REF_DIA_MAX) - REF_DIA_MIN;

  String falhas  = "";
  String alertas = "";

  for (int i = 0; i < NUM_SENSORES_REF; i++) {
    float v = x[i];
    if (isnan(v) || v == 0) {
      if (falhas.length() > 0) falhas += ", ";
      falhas += NOMES_SENSORES_REF[i];
      continue;
    }

    float media = REF_MEDIA_DIA[idxDia][i];
    float dp    = REF_DP_DIA[idxDia][i];
    float z     = (dp > 0) ? (v - media) / dp : 0;

    const char* situacao = nullptr;
    bool alto = false;

    if (v < REF_MIN[i])      { situacao = "abaixo de tudo no dataset"; d.extrapolando = true; }
    else if (v > REF_MAX[i]) { situacao = "acima de tudo no dataset";  alto = true; d.extrapolando = true; }
    else if (v < REF_P5[i])  { situacao = "abaixo da faixa das plantas saudáveis"; }
    else if (v > REF_P95[i]) { situacao = "acima da faixa das plantas saudáveis"; alto = true; }
    else if (fabs(z) > 2)    { situacao = "incomum para plantas nessa fase"; alto = (z > 0); }

    if (situacao != nullptr) {
      d.numAlertas++;
      alertas += "; ";
      alertas += NOMES_SENSORES_REF[i];
      alertas += " ";
      alertas += String(v, (i == 3) ? 2 : 1);
      alertas += UNIDADES_REF[i];
      alertas += " ";
      alertas += situacao;
      alertas += " -> ";
      alertas += alto ? DICA_ALTA[i] : DICA_BAIXA[i];
    }
  }

  d.confiavel = (falhas.length() == 0);
  if (!d.confiavel) {
    d.numAlertas++;
    d.conclusao = "LEITURA NÃO CONFIÁVEL (sensor desconectado: " + falhas + "). ";
  }
  d.conclusao += NOMES_STATUS_ALFACE[d.status];
  d.conclusao += " (" + String((int)lround(d.confianca * 100)) + "%)";
  if (d.confiavel && d.status == 2) d.conclusao += ": verificar a torre";
  if (d.extrapolando) d.conclusao += "; valores fora do dataset, previsão extrapolada";
  if (d.confiavel && d.numAlertas == 0) d.conclusao += "; sensores dentro do padrão do dataset";
  d.conclusao += alertas;
  return d;
}

void imprimirDiagnostico(const DiagnosticoAlface& d) {
  Serial.print("[MODELO] ");
  if (d.valido) {
    Serial.print("Dia ");
    Serial.print(d.dias);
    Serial.print(" -> ");
  }
  Serial.println(d.conclusao);
}

// =========================================================================
// 4. SINCRONIZAÇÃO DE HORA (NTP -> PICO) E INDICADORES LED
// =========================================================================

bool enviarHoraParaPico(uint32_t timeoutMs) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, timeoutMs)) {
    Serial.println("[NTP] Relógio ainda não sincronizado");
    return false;
  }

  char bufferHora[10];
  snprintf(bufferHora, sizeof(bufferHora), "T:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  Serial2.println(bufferHora);

  Serial.print("[NTP] Hora enviada para a BitDogLab: ");
  Serial.println(bufferHora);
  return true;
}

void piscarLedSerial() {
  digitalWrite(LED_SERIAL, HIGH);
  ledSerialDesligaEm = millis() + TEMPO_LED_SERIAL;
}

void atualizarLeds() {
  if (ledSerialDesligaEm != 0 && millis() >= ledSerialDesligaEm) {
    digitalWrite(LED_SERIAL, LOW);
    ledSerialDesligaEm = 0;
  }

  if (WiFi.status() == WL_CONNECTED) {
    digitalWrite(LED_WIFI, HIGH);
  } else if (millis() - ultimaPiscadaWifi >= 300) {
    ultimaPiscadaWifi = millis();
    estadoLedWifi = !estadoLedWifi;
    digitalWrite(LED_WIFI, estadoLedWifi);
  }
}

// =========================================================================
// 5. COMUNICAÇÃO COM O THINGSBOARD (ATRIBUTOS E TELEMETRIA)
// =========================================================================

void sincronizarAtributosDashboard() {
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;

  http.begin(cliente, urlAtributos);
  int resultado = http.GET();

  if (resultado == 200) {
    String resposta = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, resposta);

    if (!error) {
      JsonVariantConst dataPlantio = doc["shared"]["data_plantio"].as<JsonVariantConst>();
      if (!dataPlantio.isNull()) {
        atualizarDataPlantio(dataPlantio);
      }
    } else {
      Serial.print("[ERRO JSON ATRIBUTOS] ");
      Serial.println(error.f_str());
    }
  } else {
    Serial.printf("[HTTP GET ATRIBUTOS] Erro: %d\n", resultado);
  }

  http.end();
}

void processarTelemetriaDaPico() {
  // Esvazia o buffer da Serial2 (que chega a cada 500ms) guardando sempre o pacote mais recente
  String ultimoPacoteValido = "";
  while (Serial2.available() > 0) {
    String linha = Serial2.readStringUntil('\n');
    linha.trim();
    if (linha.startsWith("B:")) {
      ultimoPacoteValido = linha;
    }
  }

  if (ultimoPacoteValido.length() == 0) return;

  // Variáveis de leitura do pacote da BitDogLab (Sem PWM 'M:')
  float tensao_bat_lida   = 0.0f;
  int   luminosidade_lida = 0;
  float temp_agua_lida    = 0.0f;
  float temp_ar_lida      = 0.0f;
  int   umidade_lida      = 0;
  float ph_lido           = 0.0f;
  int   tds_lido          = 0;
  int   modo_auto_lido    = 0;
  int   estado_bomba_lido = 0;
  int   min_ciclo_lido    = 0;
  int   tam_ciclo_lido    = 0;
  int   hora_pico         = 0;
  int   min_pico          = 0;

  bool temEstadoECiclo = false;
  bool pacoteValido    = false;

  // 1ª Tentativa: Formato novo completo do comunicacao.cpp (13 valores, com E: e C: após A:)
  // Exemplo: B:12.2,L:80,TA:24.5,TR:26.0,U:65,P:6.20,S:650,A:1,E:1,C:3/15,T:14:35
  int lidos = sscanf(ultimoPacoteValido.c_str(),
                     "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,A:%d,E:%d,C:%d/%d,T:%d:%d",
                     &tensao_bat_lida, &luminosidade_lida, &temp_agua_lida,
                     &temp_ar_lida, &umidade_lida, &ph_lido, &tds_lido,
                     &modo_auto_lido, &estado_bomba_lido, &min_ciclo_lido, &tam_ciclo_lido,
                     &hora_pico, &min_pico);

  if (lidos == 13) {
    pacoteValido    = true;
    temEstadoECiclo = true;
  } else {
    // 2ª Tentativa: Variação com E: e C: antes de A: (13 valores)
    lidos = sscanf(ultimoPacoteValido.c_str(),
                   "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,E:%d,C:%d/%d,A:%d,T:%d:%d",
                   &tensao_bat_lida, &luminosidade_lida, &temp_agua_lida,
                   &temp_ar_lida, &umidade_lida, &ph_lido, &tds_lido,
                   &estado_bomba_lido, &min_ciclo_lido, &tam_ciclo_lido,
                   &modo_auto_lido, &hora_pico, &min_pico);
    if (lidos == 13) {
      pacoteValido    = true;
      temEstadoECiclo = true;
    } else {
      // 3ª Tentativa: Formato curto sem E: e C: (10 valores)
      // Exemplo: B:12.2,L:80,TA:24.5,TR:26.0,U:65,P:6.20,S:650,A:1,T:14:35
      lidos = sscanf(ultimoPacoteValido.c_str(),
                     "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,A:%d,T:%d:%d",
                     &tensao_bat_lida, &luminosidade_lida, &temp_agua_lida,
                     &temp_ar_lida, &umidade_lida, &ph_lido, &tds_lido,
                     &modo_auto_lido, &hora_pico, &min_pico);
      if (lidos == 10) {
        pacoteValido    = true;
        temEstadoECiclo = false;
      }
    }
  }

  if (!pacoteValido) {
    Serial.printf("[SERIAL2] Pacote ignorado (%d campos lidos): [%s]\n", lidos, ultimoPacoteValido.c_str());
    return;
  }

  piscarLedSerial();

  // Respeita o intervalo de envio HTTP (2s) para não travar a ESP32 com TLS a cada 500ms
  if (millis() - ultimoPostTelemetria < intervaloPostTb) {
    return;
  }
  ultimoPostTelemetria = millis();

  Serial.print("\n[PICO RX] ");
  Serial.println(ultimoPacoteValido);

  // Executa o modelo de Árvore de Decisão localmente
  DiagnosticoAlface diag = analisarAlface(temp_ar_lida, (float)umidade_lida, (float)tds_lido, ph_lido);
  imprimirDiagnostico(diag);

  if (WiFi.status() != WL_CONNECTED) return;

  // Monta e envia o JSON para o ThingsBoard
  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;

  http.begin(cliente, server);
  http.addHeader("Content-Type", "application/json");

  char hora_str[6];
  snprintf(hora_str, sizeof(hora_str), "%02d:%02d", hora_pico, min_pico);

  JsonDocument pacote;
  pacote["luminosidade"]       = luminosidade_lida;
  pacote["temp_agua"]          = temp_agua_lida;
  pacote["temp_ar"]            = temp_ar_lida;
  pacote["umidade"]            = umidade_lida;
  pacote["nivel_bateria"]      = tensao_bat_lida;
  pacote["ph"]                 = ph_lido;
  pacote["ec"]                 = tds_lido;
  pacote["nivel_reservatorio"] = 100;

  // Estado do Relé da Bomba e Progresso do Ciclo
  if (temEstadoECiclo) {
    bool bombaLigada = (estado_bomba_lido == 1);
    char ciclo_str[16];
    snprintf(ciclo_str, sizeof(ciclo_str), "%d/%d", min_ciclo_lido, tam_ciclo_lido);

    pacote["bomba_ativa"]   = bombaLigada;
    pacote["estado_bomba"]  = bombaLigada ? "Ligada" : "Desligada";
    pacote["forca_bomba"]   = bombaLigada ? 100 : 0; // Relé ON = 100%, OFF = 0%
    pacote["ciclo_bomba"]   = ciclo_str;
    pacote["minuto_ciclo"]  = min_ciclo_lido;
    pacote["tamanho_ciclo"] = tam_ciclo_lido;
  }

  // Modo de Operação e Horário da BitDogLab
  pacote["modo_automatico"] = (modo_auto_lido == 1);
  pacote["modo_operacao"]   = (modo_auto_lido == 1) ? "Automático" : "Manual";
  pacote["hora_pico"]       = hora_str;

  // Resultados do Modelo de Predição (Árvore de Decisão)
  if (diag.valido) {
    pacote["dias_crescimento"]   = diag.dias;
    pacote["status_alface"]      = diag.status;
    pacote["status_alface_nome"] = NOMES_STATUS_ALFACE[diag.status];
    pacote["confianca_modelo"]   = diag.confianca;
    pacote["leitura_confiavel"]  = diag.confiavel;
    pacote["alertas_alface"]     = diag.numAlertas;
  }
  pacote["conclusao_alface"] = diag.conclusao;

  char plantio_str[16];
  snprintf(plantio_str, sizeof(plantio_str), "%02d/%02d/%04d", plantioDia, plantioMes, plantioAno);
  pacote["data_plantio_em_uso"] = plantio_str;
  pacote["data_plantio_origem"] = plantioVeioDoDashboard ? "dashboard" : "código";

  String payload;
  serializeJson(pacote, payload);

  int codigoHttp = http.POST(payload);
  if (codigoHttp == 200) {
    Serial.println("[THINGSBOARD] Telemetria + Predição enviadas com sucesso (HTTP 200).");
  } else {
    Serial.printf("[THINGSBOARD] Erro no envio HTTP: %d\n", codigoHttp);
  }
  http.end();
}

// =========================================================================
// 6. SETUP E LOOP PRINCIPAL DA ESP32
// =========================================================================

void setup() {
  Serial.begin(115200);

  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_SERIAL, OUTPUT);
  digitalWrite(LED_WIFI, LOW);
  digitalWrite(LED_SERIAL, LOW);

  Serial2.setRxBufferSize(1024);
  Serial2.begin(BAUD_PICO, SERIAL_8N1, RXp2, TXp2);

  WiFi.begin(ssid, senha);
  Serial.print("[WIFI] Conectando a ");
  Serial.println(ssid);

  while (WiFi.status() != WL_CONNECTED) {
    atualizarLeds();
    delay(100);
    Serial.print(".");
  }

  atualizarLeds();
  Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());

  Serial.println("[NTP] Sincronizando relógio...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  horaEnviada = enviarHoraParaPico(5000);
  ultimoEnvioHora = millis();
}

void loop() {
  atualizarLeds();

  // 1. Processa pacotes recebidos da BitDogLab na Serial2
  if (Serial2.available() > 0) {
    processarTelemetriaDaPico();
  }

  // 2. Consulta o atributo "data_plantio" no ThingsBoard a cada 5 segundos
  if (millis() - ultimaSincronizacaoAtributos >= intervaloAtributos) {
    ultimaSincronizacaoAtributos = millis();
    sincronizarAtributosDashboard();
  }

  // 3. Envia a hora "T:HH:MM" para a BitDogLab a cada 60s (ou 5s até sincronizar o NTP)
  unsigned long intervaloHora = horaEnviada ? intervaloEnvioHora : intervaloRetentativaHora;
  if (WiFi.status() == WL_CONNECTED && millis() - ultimoEnvioHora >= intervaloHora) {
    ultimoEnvioHora = millis();
    horaEnviada = enviarHoraParaPico(200);
  }
}