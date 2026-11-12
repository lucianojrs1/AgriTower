#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include "time.h"

// ---------- Pinos e configuração da serial com a BitDogLab ----------
#define RXp2 16            // RX da ESP32 <- TX da BitDogLab (GP8)
#define TXp2 17            // TX da ESP32 -> RX da BitDogLab (GP9)
#define BAUD_PICO 9600     // mesmo baud do código da BitDogLab

// ---------- LEDs de status ----------
#define LED_WIFI   25      // indica estado do Wi-Fi
#define LED_SERIAL 26      // indica pacote válido recebido da BitDogLab
const unsigned long TEMPO_LED_SERIAL = 100; // ms aceso a cada pacote

int ultimoEnvio = -1; // variável que salva a última modificação no thingsboard
int valorAtual = -1;
const char* ssid = "CINGUESTS";
const char* senha = "acessocin";
const char* server = "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/telemetry";

unsigned long ledSerialDesligaEm = 0;
unsigned long ultimaPiscadaWifi = 0;
bool estadoLedWifi = false;

// ---------- NTP (hora enviada à BitDogLab) ----------
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -10800;   // Recife/Brasília (GMT-3)
const int   daylightOffset_sec = 0;

unsigned long ultimoEnvioHora = 0;
const unsigned long intervaloEnvioHora = 60000;      // envia a hora a cada 60 s
const unsigned long intervaloRetentativaHora = 5000; // enquanto o NTP não sincronizou
bool horaEnviada = false;  // vira true após o primeiro envio bem-sucedido

// ######################################################################
// ##### INICIO DO MODELO DE ÁRVORE DE DECISÃO + REFERÊNCIA DO DATASET #####
// ######################################################################

// =====================================================================
// MODELO DE ÁRVORE DE DECISÃO (mesmo treino do notebook modelo_esc.ipynb)
// DecisionTreeClassifier(max_depth=5, random_state=42) no lettuce_dataset.csv
// Entradas, nesta ordem: x[0]=Temperatura (°C), x[1]=Umidade (%),
//                        x[2]=TDS (ppm), x[3]=pH, x[4]=Dias de crescimento
// Saída: 0 = Crescendo, 1 = Pronto para colher, 2 = Estragado
// =====================================================================
namespace Eloquent {
  namespace ML {
    namespace Port {
      class DecisionTree {
        public:
          float confianca = 0; // fração das amostras de treino na folha que têm a classe prevista

          int predict(float *x) {
            if (x[4] <= 34.500000f) {
                if (x[0] <= 32.850000f) {
                    confianca = 1.0000f;
                    return 0;
                }
                else {
                    if (x[3] <= 6.150000f) {
                        confianca = 1.0000f;
                        return 2;
                    }
                    else {
                        if (x[3] <= 6.650000f) {
                            confianca = 1.0000f;
                            return 0;
                        }
                        else {
                            confianca = 1.0000f;
                            return 2;
                        }
                    }
                }
            }
            else {
                if (x[4] <= 42.500000f) {
                    confianca = 1.0000f;
                    return 1;
                }
                else {
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

// =====================================================================
// REFERÊNCIA DO DATASET (lettuce_dataset.csv, 3169 registros)
// Índices dos sensores: 0=Temperatura, 1=Umidade, 2=TDS, 3=pH
// =====================================================================
const int NUM_SENSORES_REF = 4;
const char* NOMES_SENSORES_REF[NUM_SENSORES_REF] = {"Temperatura", "Umidade", "TDS", "pH"};
const char* UNIDADES_REF[NUM_SENSORES_REF] = {" °C", " %", " ppm", ""};

// Menor e maior valor que existe no dataset
const float REF_MIN[NUM_SENSORES_REF] = {18.00f, 50.00f, 400.00f, 6.00f};
const float REF_MAX[NUM_SENSORES_REF] = {33.50f, 80.00f, 800.00f, 6.80f};

// Faixa típica das plantas saudáveis (não estragadas): percentis 5 e 95
const float REF_P5[NUM_SENSORES_REF]  = {19.10f, 51.00f, 418.00f, 6.00f};
const float REF_P95[NUM_SENSORES_REF] = {33.40f, 79.00f, 781.35f, 6.80f};

// Média e desvio padrão das plantas na mesma fase (dia ± 2), dias 1 a 48
const int REF_DIA_MIN = 1;
const int REF_DIA_MAX = 48;
const float REF_MEDIA_DIA[48][NUM_SENSORES_REF] = {
  {30.07f, 64.90f, 602.59f, 6.39f},  // dia 1
  {30.07f, 64.69f, 602.92f, 6.38f},  // dia 2
  {30.10f, 64.65f, 603.22f, 6.38f},  // dia 3
  {29.97f, 64.91f, 599.79f, 6.38f},  // dia 4
  {29.78f, 65.23f, 597.47f, 6.37f},  // dia 5
  {28.28f, 65.37f, 602.27f, 6.38f},  // dia 6
  {28.04f, 66.00f, 602.23f, 6.39f},  // dia 7
  {27.58f, 66.13f, 600.21f, 6.41f},  // dia 8
  {27.25f, 66.23f, 600.99f, 6.41f},  // dia 9
  {26.97f, 65.83f, 604.68f, 6.41f},  // dia 10
  {28.02f, 65.43f, 599.59f, 6.41f},  // dia 11
  {27.71f, 64.55f, 600.97f, 6.40f},  // dia 12
  {27.62f, 64.54f, 597.36f, 6.40f},  // dia 13
  {27.75f, 64.29f, 600.88f, 6.41f},  // dia 14
  {27.80f, 64.39f, 595.17f, 6.41f},  // dia 15
  {27.95f, 64.47f, 596.74f, 6.42f},  // dia 16
  {28.08f, 64.97f, 595.47f, 6.42f},  // dia 17
  {28.22f, 64.61f, 605.01f, 6.41f},  // dia 18
  {28.22f, 64.72f, 601.95f, 6.40f},  // dia 19
  {28.32f, 64.70f, 601.52f, 6.39f},  // dia 20
  {28.40f, 64.74f, 599.30f, 6.40f},  // dia 21
  {28.55f, 64.47f, 602.60f, 6.40f},  // dia 22
  {28.47f, 65.01f, 597.64f, 6.41f},  // dia 23
  {28.20f, 64.85f, 599.51f, 6.40f},  // dia 24
  {27.79f, 64.83f, 605.39f, 6.40f},  // dia 25
  {27.47f, 64.91f, 604.49f, 6.40f},  // dia 26
  {27.12f, 64.77f, 600.49f, 6.40f},  // dia 27
  {27.39f, 63.97f, 602.62f, 6.41f},  // dia 28
  {27.78f, 64.35f, 601.08f, 6.42f},  // dia 29
  {28.25f, 64.24f, 595.97f, 6.42f},  // dia 30
  {28.57f, 64.09f, 589.53f, 6.41f},  // dia 31
  {29.07f, 64.57f, 590.53f, 6.42f},  // dia 32
  {29.08f, 65.53f, 582.79f, 6.41f},  // dia 33
  {29.00f, 65.33f, 582.10f, 6.39f},  // dia 34
  {28.61f, 65.86f, 581.75f, 6.39f},  // dia 35
  {26.96f, 65.89f, 585.90f, 6.39f},  // dia 36
  {26.71f, 65.52f, 581.45f, 6.37f},  // dia 37
  {26.60f, 65.02f, 588.91f, 6.38f},  // dia 38
  {26.61f, 64.65f, 591.67f, 6.40f},  // dia 39
  {26.79f, 64.20f, 593.92f, 6.39f},  // dia 40
  {28.09f, 63.97f, 599.59f, 6.40f},  // dia 41
  {28.09f, 63.94f, 603.68f, 6.40f},  // dia 42
  {28.13f, 64.47f, 605.30f, 6.40f},  // dia 43
  {28.05f, 64.84f, 603.98f, 6.39f},  // dia 44
  {28.17f, 64.74f, 605.78f, 6.40f},  // dia 45
  {28.68f, 65.14f, 607.36f, 6.39f},  // dia 46
  {29.01f, 66.25f, 609.98f, 6.41f},  // dia 47
  {29.93f, 63.50f, 585.28f, 6.38f}   // dia 48
};
const float REF_DP_DIA[48][NUM_SENSORES_REF] = {
  {5.63f, 8.89f, 106.84f, 0.24f},  // dia 1
  {5.62f, 8.82f, 105.46f, 0.24f},  // dia 2
  {5.57f, 8.98f, 107.33f, 0.24f},  // dia 3
  {5.42f, 8.93f, 109.44f, 0.24f},  // dia 4
  {5.33f, 8.85f, 111.06f, 0.23f},  // dia 5
  {5.53f, 8.86f, 112.00f, 0.23f},  // dia 6
  {5.37f, 8.87f, 114.25f, 0.23f},  // dia 7
  {5.01f, 8.80f, 116.35f, 0.23f},  // dia 8
  {4.77f, 8.87f, 118.32f, 0.23f},  // dia 9
  {4.47f, 8.93f, 116.93f, 0.24f},  // dia 10
  {4.21f, 9.04f, 118.74f, 0.24f},  // dia 11
  {3.99f, 9.02f, 120.69f, 0.24f},  // dia 12
  {3.96f, 9.02f, 119.65f, 0.24f},  // dia 13
  {4.03f, 8.96f, 119.68f, 0.24f},  // dia 14
  {4.15f, 9.13f, 120.17f, 0.24f},  // dia 15
  {4.18f, 9.02f, 118.91f, 0.23f},  // dia 16
  {4.10f, 9.01f, 116.24f, 0.23f},  // dia 17
  {4.14f, 8.95f, 116.19f, 0.23f},  // dia 18
  {4.10f, 8.89f, 115.96f, 0.23f},  // dia 19
  {4.11f, 8.81f, 116.18f, 0.23f},  // dia 20
  {4.16f, 9.05f, 115.63f, 0.23f},  // dia 21
  {4.28f, 9.12f, 116.00f, 0.23f},  // dia 22
  {4.28f, 9.14f, 114.47f, 0.23f},  // dia 23
  {4.31f, 9.32f, 112.92f, 0.23f},  // dia 24
  {4.18f, 9.10f, 111.69f, 0.23f},  // dia 25
  {3.95f, 8.98f, 114.27f, 0.23f},  // dia 26
  {3.78f, 9.02f, 116.06f, 0.23f},  // dia 27
  {3.96f, 9.15f, 118.06f, 0.23f},  // dia 28
  {4.12f, 9.01f, 119.14f, 0.22f},  // dia 29
  {4.29f, 9.16f, 121.55f, 0.22f},  // dia 30
  {4.56f, 8.88f, 119.85f, 0.23f},  // dia 31
  {4.55f, 8.76f, 120.73f, 0.23f},  // dia 32
  {4.63f, 8.54f, 117.99f, 0.24f},  // dia 33
  {4.70f, 8.68f, 115.46f, 0.24f},  // dia 34
  {4.75f, 8.76f, 112.01f, 0.24f},  // dia 35
  {5.36f, 9.01f, 114.01f, 0.24f},  // dia 36
  {5.36f, 9.26f, 111.77f, 0.24f},  // dia 37
  {5.24f, 9.31f, 112.37f, 0.24f},  // dia 38
  {5.14f, 9.32f, 116.60f, 0.24f},  // dia 39
  {5.11f, 9.26f, 118.85f, 0.24f},  // dia 40
  {4.30f, 9.21f, 119.22f, 0.24f},  // dia 41
  {4.14f, 9.04f, 118.75f, 0.24f},  // dia 42
  {4.15f, 8.95f, 119.63f, 0.24f},  // dia 43
  {4.09f, 8.84f, 116.59f, 0.24f},  // dia 44
  {4.04f, 8.76f, 115.47f, 0.24f},  // dia 45
  {4.13f, 8.66f, 110.95f, 0.24f},  // dia 46
  {4.19f, 8.45f, 108.07f, 0.23f},  // dia 47
  {2.81f, 8.10f, 71.20f, 0.22f}   // dia 48
};

// ---------- Data de plantio (o modelo precisa dos "dias de crescimento") ----------
const int PLANTIO_ANO = 2026;
const int PLANTIO_MES = 10;
const int PLANTIO_DIA = 1;
const int DIAS_CRESCIMENTO_MANUAL = -1; // coloque um valor >= 1 para forçar os dias (ex.: testar sem NTP)

// A data de plantio pode ser definida pelo dashboard (atributo compartilhado "data_plantio").
// As constantes acima são só o valor padrão, usado até o ThingsBoard mandar uma data.
int plantioAno = PLANTIO_ANO;
int plantioMes = PLANTIO_MES;
int plantioDia = PLANTIO_DIA;
bool plantioVeioDoDashboard = false;
const int DIAS_SEM_RELOGIO = -9999; // resposta de calcularDiasCrescimento() quando o NTP não sincronizou

const char* NOMES_STATUS_ALFACE[3] = {"Crescendo", "Pronto para colher", "Estragado"};
const char* DICA_ALTA[NUM_SENSORES_REF]  = {"ventilar/sombrear a torre", "melhorar a ventilação (risco de fungos)",
                                            "diluir a solução com água", "baixar o pH da solução"};
const char* DICA_BAIXA[NUM_SENSORES_REF] = {"proteger a torre do frio", "ar seco, a planta perde mais água",
                                            "repor a solução nutritiva", "subir o pH da solução"};

struct DiagnosticoAlface
{
  bool valido = false;       // false se não deu para calcular os dias (relógio sem NTP)
  int status = -1;           // 0 = Crescendo, 1 = Pronto para colher, 2 = Estragado
  float confianca = 0;
  int dias = -1;
  bool confiavel = true;     // false se algum sensor mandou 0 (desconectado)
  bool extrapolando = false; // true se algum valor está fora do que existe no dataset
  int numAlertas = 0;
  String conclusao;
};

// Dias desde o plantio (dia do plantio = 1). Retorna DIAS_SEM_RELOGIO se o relógio ainda não sincronizou.
int calcularDiasCrescimento()
{
  if (DIAS_CRESCIMENTO_MANUAL >= 1) return DIAS_CRESCIMENTO_MANUAL;

  time_t agora = time(nullptr);
  if (agora < 1700000000) return DIAS_SEM_RELOGIO; // NTP ainda não sincronizou

  struct tm hoje;
  localtime_r(&agora, &hoje);
  hoje.tm_hour = 12; hoje.tm_min = 0; hoje.tm_sec = 0; // compara só as datas

  struct tm plantio = {};
  plantio.tm_year = plantioAno - 1900;
  plantio.tm_mon  = plantioMes - 1;
  plantio.tm_mday = plantioDia;
  plantio.tm_hour = 12;
  plantio.tm_isdst = -1;

  double segundos = difftime(mktime(&hoje), mktime(&plantio));
  return (int)lround(segundos / 86400.0) + 1;
}

// Lê a data de plantio que veio do dashboard. Aceita:
//  - texto "AAAA-MM-DD" ou "DD/MM/AAAA" (widget/atributo de texto)
//  - número em milissegundos (widget de data do ThingsBoard)
bool atualizarDataPlantio(JsonVariantConst valor)
{
  int ano = 0, mes = 0, dia = 0;
  if (valor.is<const char*>())
  {
    const char* texto = valor.as<const char*>();
    if (sscanf(texto, "%d-%d-%d", &ano, &mes, &dia) != 3 &&
        sscanf(texto, "%d/%d/%d", &dia, &mes, &ano) != 3)
    {
      ano = 0; // formato desconhecido
    }
  }
  else if (valor.is<double>())
  {
    time_t segundos = (time_t)(valor.as<double>() / 1000.0);
    struct tm data;
    localtime_r(&segundos, &data);
    ano = data.tm_year + 1900;
    mes = data.tm_mon + 1;
    dia = data.tm_mday;
  }

  if (ano < 2020 || ano > 2100 || mes < 1 || mes > 12 || dia < 1 || dia > 31)
  {
    static unsigned long ultimoAviso = 0;
    if (ultimoAviso == 0 || millis() - ultimoAviso >= 60000) // avisa no máximo 1x por minuto
    {
      ultimoAviso = millis();
      Serial.println("[DASHBOARD] data_plantio inválida (use AAAA-MM-DD, DD/MM/AAAA ou o widget de data)");
    }
    return false;
  }

  if (!plantioVeioDoDashboard || ano != plantioAno || mes != plantioMes || dia != plantioDia)
  {
    Serial.printf("[DASHBOARD] Nova data de plantio: %02d/%02d/%04d\n", dia, mes, ano);
  }
  plantioAno = ano;
  plantioMes = mes;
  plantioDia = dia;
  plantioVeioDoDashboard = true;
  return true;
}

// Roda a árvore de decisão e compara a leitura com o dataset de referência
DiagnosticoAlface analisarAlface(float temperatura, float umidade, float tds, float ph)
{
  DiagnosticoAlface d;
  d.dias = calcularDiasCrescimento();
  if (d.dias == DIAS_SEM_RELOGIO)
  {
    d.conclusao = "Relógio sem NTP: não dá para calcular os dias de crescimento.";
    return d;
  }
  if (d.dias < 1)
  {
    d.conclusao = "Data de plantio depois de hoje: confira o campo data_plantio no dashboard.";
    return d;
  }
  d.valido = true;

  // 1) Árvore de decisão (mesma ordem de entradas do treino)
  float x[5] = {temperatura, umidade, tds, ph, (float)d.dias};
  d.status = modeloAlface.predict(x);
  d.confianca = modeloAlface.confianca;

  // 2) Comparação com o dataset
  d.extrapolando = (d.dias < REF_DIA_MIN || d.dias > REF_DIA_MAX);
  int idxDia = constrain(d.dias, REF_DIA_MIN, REF_DIA_MAX) - REF_DIA_MIN; // fase mais próxima

  String falhas = "";
  String alertas = "";
  for (int i = 0; i < NUM_SENSORES_REF; i++)
  {
    float v = x[i];
    if (isnan(v) || v == 0) // o firmware da BitDogLab manda 0 quando o sensor falha
    {
      if (falhas.length() > 0) falhas += ", ";
      falhas += NOMES_SENSORES_REF[i];
      continue;
    }

    float media = REF_MEDIA_DIA[idxDia][i];
    float dp = REF_DP_DIA[idxDia][i];
    float z = (dp > 0) ? (v - media) / dp : 0;

    const char* situacao = nullptr;
    bool alto = false;
    if (v < REF_MIN[i])       { situacao = "abaixo de tudo no dataset"; d.extrapolando = true; }
    else if (v > REF_MAX[i])  { situacao = "acima de tudo no dataset"; alto = true; d.extrapolando = true; }
    else if (v < REF_P5[i])   { situacao = "abaixo da faixa das plantas saudáveis"; }
    else if (v > REF_P95[i])  { situacao = "acima da faixa das plantas saudáveis"; alto = true; }
    else if (fabs(z) > 2)     { situacao = "incomum para plantas nessa fase"; alto = (z > 0); }

    if (situacao != nullptr)
    {
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

  // 3) Conclusão
  d.confiavel = (falhas.length() == 0);
  if (!d.confiavel)
  {
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

void imprimirDiagnostico(const DiagnosticoAlface& d)
{
  Serial.print("[MODELO] ");
  if (d.valido)
  {
    Serial.print("Dia ");
    Serial.print(d.dias);
    Serial.print(" -> ");
  }
  Serial.println(d.conclusao);
}

// ######################################################################
// ##### FIM DO MODELO DE ÁRVORE DE DECISÃO ##############################
// ######################################################################

// Envia "T:HH:MM\n" para a BitDogLab (ajustado para o formato da Tarefa 1 da Pico)
bool enviarHoraParaPico(uint32_t timeoutMs)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, timeoutMs))
  {
    Serial.println("[NTP] Relógio ainda não sincronizado");
    return false;
  }

  char bufferHora[10];
  snprintf(bufferHora, sizeof(bufferHora), "T:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  Serial2.println(bufferHora);

  Serial.print("[NTP] Hora enviada para a Pico: ");
  Serial.println(bufferHora);
  return true;
}

// Acende o LED da serial e agenda o apagamento (não bloqueante)
void piscarLedSerial()
{
  digitalWrite(LED_SERIAL, HIGH);
  ledSerialDesligaEm = millis() + TEMPO_LED_SERIAL;
}

// Atualiza os LEDs; chamar sempre que possível (loop e setup)
void atualizarLeds()
{
  // LED serial: apaga quando o tempo acabar
  if (ledSerialDesligaEm != 0 && millis() >= ledSerialDesligaEm)
  {
    digitalWrite(LED_SERIAL, LOW);
    ledSerialDesligaEm = 0;
  }

  // LED Wi-Fi: aceso fixo = conectado, piscando = sem conexão
  if (WiFi.status() == WL_CONNECTED)
  {
    digitalWrite(LED_WIFI, HIGH);
  }
  else if (millis() - ultimaPiscadaWifi >= 300)
  {
    ultimaPiscadaWifi = millis();
    estadoLedWifi = !estadoLedWifi;
    digitalWrite(LED_WIFI, estadoLedWifi);
  }
}

void sincronizarBomba()
{
  if (WiFi.status() == WL_CONNECTED)
  {
    WiFiClientSecure cliente;
    cliente.setInsecure();
    HTTPClient http;

    http.begin(cliente, "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/attributes?sharedKeys=bomba_ativa,forca_bomba,data_plantio");
    int resultado = http.GET();

    if (resultado == 200)
    {
      String resposta = http.getString();
      JsonDocument doc;
      DeserializationError error = deserializeJson(doc, resposta);

      if (error)
      {
        Serial.print("Falha na leitura JSON: ");
        Serial.println(error.f_str());
        http.end();
        return;
      }

      // Data de plantio definida pelo dashboard (atributo compartilhado "data_plantio")
      JsonVariantConst dataPlantio = doc["shared"]["data_plantio"].as<JsonVariantConst>();
      if (!dataPlantio.isNull())
      {
        atualizarDataPlantio(dataPlantio);
      }

      bool ativa = doc["shared"]["bomba_ativa"];
      int forca_bomba = doc["shared"]["forca_bomba"];

      if (ativa == true)
      {
        valorAtual = forca_bomba;
      }
      else
      {
        valorAtual = 0;
      }

      if (valorAtual != ultimoEnvio)
      {
        // Nota: A BitDogLab atual ignora números puros na Serial2 (só lê "T:HH:MM"),
        // mas mantemos o envio caso você adicione controle remoto na Pico depois.
        Serial2.println(valorAtual);
        ultimoEnvio = valorAtual;
      }
    }
    else
    {
      Serial.print("Erro HTTP GET Attributes: ");
      Serial.println(resultado);
    }

    http.end();
  }
}

// Função auxiliar para enviar rapidamente eventos de transição da bomba ao ThingsBoard
void enviarEventoBomba(bool ligada, const char* textoEstado)
{
  if (WiFi.status() != WL_CONNECTED) return;

  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;

  http.begin(cliente, server);
  http.addHeader("Content-Type", "application/json");

  JsonDocument doc;
  doc["bomba_ativa"] = ligada;
  doc["estado_bomba"] = textoEstado;

  String payload;
  serializeJson(doc, payload);
  int codigo = http.POST(payload);
  Serial.printf("[EVENTO] Estado '%s' enviado ao ThingsBoard (HTTP %d)\n", textoEstado, codigo);
  http.end();
}

void enviarTelemetria()
{
  // 1) Lê a linha da Serial2 ANTES de abrir conexão HTTP
  String pacote_serial = Serial2.readStringUntil('\n');
  pacote_serial.trim();

  if (pacote_serial.length() == 0) return;

  // Trata as mensagens de evento imediatas enviadas pela Tarefa 3 da BitDogLab
  if (pacote_serial == "EVENTO:BOMBA_LIGANDO")
  {
    piscarLedSerial();
    Serial.println("[PICO EVENTO] Bomba iniciando rampa de subida!");
    enviarEventoBomba(true, "Ligada");
    return;
  }
  else if (pacote_serial == "EVENTO:BOMBA_DESLIGANDO")
  {
    piscarLedSerial();
    Serial.println("[PICO EVENTO] Bomba iniciando rampa de descida!");
    enviarEventoBomba(false, "Desligada");
    return;
  }

  // Variáveis para armazenar os 14 valores do pacote de telemetria da BitDogLab
  float tensao_bat_lida = 0.0;
  int luminosidade_lida = 0;
  float temp_agua_lida = 0.0;
  float temp_ar_lida = 0.0;
  int umidade_lida = 0;
  float ph_lido = 0.0;
  int tds_lido = 0;
  int pwm_lido = 0;
  int estado_bomba_lido = 0; // Novo: E:0 ou E:1
  int min_ciclo_lido = 0;    // Novo: C:minutoAtual/...
  int tam_ciclo_lido = 0;    // Novo: .../tamanhoTotal
  int modo_auto_lido = 0;
  int hora_pico = 0;
  int min_pico = 0;

  // Formato enviado pela BitDogLab:
  // B:12.3,L:45,TA:25.1,TR:27.4,U:60,P:6.85,S:420,M:255,E:1,C:3/15,A:1,T:15:30
  int itens_lidos = sscanf(pacote_serial.c_str(),
                           "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,M:%d,E:%d,C:%d/%d,A:%d,T:%d:%d",
                           &tensao_bat_lida, &luminosidade_lida, &temp_agua_lida,
                           &temp_ar_lida, &umidade_lida, &ph_lido, &tds_lido,
                           &pwm_lido, &estado_bomba_lido, &min_ciclo_lido, &tam_ciclo_lido,
                           &modo_auto_lido, &hora_pico, &min_pico);

  // A BitDogLab atual (comunicacao.cpp) manda o pacote SEM os campos E: e C:
  // B:12.3,L:45,TA:25.1,TR:27.4,U:60,P:6.85,S:420,M:255,A:1,T:15:30
  // Nesse caso o sscanf acima para em 8 campos; tenta o formato curto (11 campos).
  bool pacoteCompleto = (itens_lidos == 14);
  bool pacoteCurto = false;
  if (!pacoteCompleto)
  {
    int itens_curtos = sscanf(pacote_serial.c_str(),
                              "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,M:%d,A:%d,T:%d:%d",
                              &tensao_bat_lida, &luminosidade_lida, &temp_agua_lida,
                              &temp_ar_lida, &umidade_lida, &ph_lido, &tds_lido,
                              &pwm_lido, &modo_auto_lido, &hora_pico, &min_pico);
    pacoteCurto = (itens_curtos == 11);
  }

  // Verifica se todos os 14 valores foram lidos
  // (ou os 11 do formato curto)
  if (itens_lidos != 14 && !pacoteCurto)
  {
    Serial.print("Pacote inválido (");
    Serial.print(itens_lidos);
    Serial.print("/14 campos lidos): [");
    Serial.print(pacote_serial);
    Serial.println("]");
    return;
  }

  piscarLedSerial(); // pacote válido recebido da BitDogLab
  Serial.print("Recebido da Pico: ");
  Serial.println(pacote_serial);

  // Árvore de decisão + comparação com o dataset (roda mesmo sem Wi-Fi)
  DiagnosticoAlface diag = analisarAlface(temp_ar_lida, (float)umidade_lida, (float)tds_lido, ph_lido);
  imprimirDiagnostico(diag);

  if (WiFi.status() != WL_CONNECTED)
  {
    return;
  }

  // 2) Monta o JSON e envia para o ThingsBoard
  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;

  http.begin(cliente, server);
  http.addHeader("Content-Type", "application/json");

  bool bombaLigada = (estado_bomba_lido == 1);

  // Formata a string do ciclo (ex: "3/15 min")
  char ciclo_str[16];
  snprintf(ciclo_str, sizeof(ciclo_str), "%d/%d", min_ciclo_lido, tam_ciclo_lido);

  // Formata a hora da Pico (ex: "15:30")
  char hora_str[6];
  snprintf(hora_str, sizeof(hora_str), "%02d:%02d", hora_pico, min_pico);

  JsonDocument pacote;
  pacote["luminosidade"] = luminosidade_lida;
  pacote["temp_agua"] = temp_agua_lida;
  pacote["temp_ar"] = temp_ar_lida;
  pacote["umidade"] = umidade_lida;
  pacote["nivel_bateria"] = tensao_bat_lida;
  pacote["ph"] = ph_lido;
  pacote["ec"] = tds_lido;
  pacote["nivel_reservatorio"] = 67;

  // Dados da Bomba (Booleano, Texto e PWM)
  // (no formato curto não vem E:, então o estado da bomba fica com os eventos EVENTO:BOMBA_*)
  if (pacoteCompleto)
  {
  pacote["bomba_ativa"] = bombaLigada;                          // true ou false
  pacote["estado_bomba"] = bombaLigada ? "Ligada" : "Desligada"; // "Ligada" ou "Desligada"
  }
  pacote["forca_bomba"] = pwm_lido;

  // Dados do Ciclo da Bomba
  // (só vêm no pacote completo, campo C:)
  if (pacoteCompleto)
  {
  pacote["ciclo_bomba"] = ciclo_str;             // Ex: "3/15" ou "12/50"
  pacote["minuto_ciclo"] = min_ciclo_lido;       // Ex: 3 (útil para gráficos/barras de progresso)
  pacote["tamanho_ciclo"] = tam_ciclo_lido;      // Ex: 15 ou 50
  }

  // Modo e Relógio
  pacote["modo_automatico"] = (modo_auto_lido == 1);
  pacote["modo_operacao"] = (modo_auto_lido == 1) ? "Automático" : "Manual";
  pacote["hora_pico"] = hora_str;

  // Resultado da árvore de decisão + comparação com o dataset
  if (diag.valido)
  {
    pacote["dias_crescimento"] = diag.dias;
    pacote["status_alface"] = diag.status;                          // 0, 1 ou 2
    pacote["status_alface_nome"] = NOMES_STATUS_ALFACE[diag.status]; // "Crescendo", ...
    pacote["confianca_modelo"] = diag.confianca;
    pacote["leitura_confiavel"] = diag.confiavel;
    pacote["alertas_alface"] = diag.numAlertas;
  }
  pacote["conclusao_alface"] = diag.conclusao;

  // Data de plantio que a ESP está usando (para conferir no dashboard)
  char plantio_str[16];
  snprintf(plantio_str, sizeof(plantio_str), "%02d/%02d/%04d", plantioDia, plantioMes, plantioAno);
  pacote["data_plantio_em_uso"] = plantio_str;
  pacote["data_plantio_origem"] = plantioVeioDoDashboard ? "dashboard" : "código";

  String serial_temp;
  serializeJson(pacote, serial_temp);

  int http_tempo_resposta = http.POST(serial_temp);

  if (http_tempo_resposta == 200)
  {
    Serial.println("Pacote enviado com sucesso ao ThingsBoard.");
  }
  else
  {
    Serial.print("Erro HTTP: ");
    Serial.println(http_tempo_resposta);
  }
  http.end();
}

void setup()
{
  Serial.begin(115200); // monitor serial do PC (independente da Serial2)

  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_SERIAL, OUTPUT);
  digitalWrite(LED_WIFI, LOW);
  digitalWrite(LED_SERIAL, LOW);

  Serial2.setRxBufferSize(1024);
  Serial2.begin(BAUD_PICO, SERIAL_8N1, RXp2, TXp2);

  WiFi.begin(ssid, senha);

  while (WiFi.status() != WL_CONNECTED)
  {
    atualizarLeds();
    delay(100);
    Serial.println("Aguardando conexão...");
  }

  atualizarLeds();
  Serial.println("Endereço IP: ");
  Serial.println(WiFi.localIP());
  Serial.println("Conectado, chefia!");

  Serial.println("Sincronizando relógio NTP...");
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  horaEnviada = enviarHoraParaPico(5000); // tenta enviar a hora logo ao ligar
  ultimoEnvioHora = millis();
}

unsigned long ultimaLeitura = 0;

void loop()
{
  atualizarLeds();

  if (millis() - ultimaLeitura >= 2000)
  {
    ultimaLeitura = millis();
    sincronizarBomba();
  }

  if (Serial2.available() > 0)
  {
    enviarTelemetria();
  }

  // Envia a hora "T:HH:MM" para a BitDogLab a cada 60 s (ou a cada 5 s até sincronizar)
  unsigned long intervaloHora = horaEnviada ? intervaloEnvioHora : intervaloRetentativaHora;
  if (WiFi.status() == WL_CONNECTED && millis() - ultimoEnvioHora >= intervaloHora)
  {
    ultimoEnvioHora = millis();
    horaEnviada = enviarHoraParaPico(200);
  }
}
