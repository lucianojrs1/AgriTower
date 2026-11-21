#pragma once
#include <Arduino.h>

// Último pacote válido recebido da BitDogLab
struct DadosTelemetriaPico {
  float  tensaoBat       = 0.0f;
  int    luminosidade    = 0;
  float  tempAgua        = 0.0f;
  float  tempAr          = 0.0f;
  int    umidade         = 0;
  float  ph              = 0.0f;
  int    tds             = 0;
  int    modoAuto        = 0;
  int    estadoBomba     = 0;
  int    minCiclo        = 0;
  int    tamCiclo        = 0;
  int    horaPico        = 0;
  int    minPico         = 0;
  bool   temEstadoECiclo = false;
  bool   disponivel      = false;
  String pacoteBruto;
};

extern DadosTelemetriaPico ultimosDadosPico;

void iniciarSerialPico();                 // Configura a UART2 (RX=16, TX=17)
void lerSerial2NaoBloqueante();           // Lê bytes e monta linhas sem bloquear
bool interpretarPacotePico(const String& linhaBruta);
void enviarLinhaPico(const char* linha);  // Envia uma linha (println) para a BitDogLab
