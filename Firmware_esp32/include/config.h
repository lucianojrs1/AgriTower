#pragma once
#include <Arduino.h>

// =========================================================================
// CONFIGURAÇÕES GERAIS: hardware, Wi-Fi, ThingsBoard, NTP e temporizadores
// =========================================================================

// --- Pinos e configuração da UART2 com a BitDogLab ---
#define RXp2      16       // RX da ESP32 <- TX da BitDogLab (GP8)
#define TXp2      17       // TX da ESP32 -> RX da BitDogLab (GP9)
#define BAUD_PICO 9600     // Mesmo baud rate do relogio.cpp / comunicacao.cpp

// --- LEDs de Status ---
#define LED_WIFI   25      // Aceso fixo = conectado | Piscando = reconectando
#define LED_SERIAL 26      // Pisca ao receber pacote válido da BitDogLab
const unsigned long TEMPO_LED_SERIAL = 100;

// --- Credenciais Wi-Fi e Endpoints ThingsBoard ---
const char* const ssid         = "CINGUESTS";
const char* const senha        = "acessocin";
const char* const server       = "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/telemetry";
const char* const urlAtributos = "https://tb.cin.ufpe.br/api/v1/torre_hidroponica_token/attributes?sharedKeys=data_plantio";

// --- Configuração NTP (Fuso Recife/Brasília GMT-3) ---
const char* const ntpServer          = "pool.ntp.org";
const long        gmtOffset_sec      = -10800;
const int         daylightOffset_sec = 0;

// --- Temporizadores de Controle (Não-Bloqueantes) ---
const unsigned long intervaloReconexaoWifi    = 15000;  // Tenta reconectar Wi-Fi a cada 15 s
const unsigned long timeoutWifiBoot           = 15000;  // Espera máxima pelo Wi-Fi no boot

const unsigned long intervaloEnvioHora        = 60000;  // Envia "T:HH:MM" a cada 60 s
const unsigned long intervaloRetentativaHora  = 5000;   // Retenta a cada 5 s enquanto sem NTP

const unsigned long intervaloAtributos        = 300000; // Consulta data_plantio a cada 5 min (300 s)

const unsigned long intervaloPostTb           = 30000;  // Envia POST HTTP ao ThingsBoard a cada 30 s
const unsigned long intervaloPostEventoMinimo = 2000;   // Intervalo mínimo para envio imediato em mudança de bomba/modo
const unsigned long intervaloRetryHttp        = 10000;  // Retenta em 10 s se houver falha HTTP

// --- Data de plantio padrão (usada até o dashboard sobrescrever) ---
const int PLANTIO_ANO = 2026;
const int PLANTIO_MES = 10;
const int PLANTIO_DIA = 1;
const int DIAS_CRESCIMENTO_MANUAL = -1; // Coloque >= 1 para forçar dias fixos em teste
const int DIAS_SEM_RELOGIO = -9999;
