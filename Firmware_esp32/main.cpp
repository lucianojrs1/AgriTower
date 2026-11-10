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

    http.begin(cliente, "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/attributes?sharedKeys=bomba_ativa,forca_bomba");
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

  // Verifica se todos os 14 valores foram lidos
  if (itens_lidos != 14)
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
  pacote["bomba_ativa"] = bombaLigada;                          // true ou false
  pacote["estado_bomba"] = bombaLigada ? "Ligada" : "Desligada"; // "Ligada" ou "Desligada"
  pacote["forca_bomba"] = pwm_lido;

  // Dados do Ciclo da Bomba
  pacote["ciclo_bomba"] = ciclo_str;             // Ex: "3/15" ou "12/50"
  pacote["minuto_ciclo"] = min_ciclo_lido;       // Ex: 3 (útil para gráficos/barras de progresso)
  pacote["tamanho_ciclo"] = tam_ciclo_lido;      // Ex: 15 ou 50

  // Modo e Relógio
  pacote["modo_automatico"] = (modo_auto_lido == 1);
  pacote["modo_operacao"] = (modo_auto_lido == 1) ? "Automático" : "Manual";
  pacote["hora_pico"] = hora_str;

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