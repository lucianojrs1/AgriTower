#include <Arduino.h>
#include "config.h"
#include "leds.h"
#include "wifi_manager.h"
#include "serial_pico.h"
#include "relogio_ntp.h"
#include "tb_atributos.h"
#include "tb_telemetria.h"

void setup() {
  Serial.begin(115200);

  iniciarLeds();
  iniciarSerialPico();

  bool wifiOk = iniciarWifi();
  iniciarNtp();
  iniciarSincronizacaoAtributos();

  if (wifiOk) {
    sincronizarHoraNoBoot();
    sincronizarAtributosNoBoot();
  }
}

void loop() {
  atualizarLeds();
  gerenciarConexaoWifi();

  // 1. Lê bytes da Serial2 sem bloquear o loop e atualiza o último pacote válido
  lerSerial2NaoBloqueante();

  // 2. Sincroniza data_plantio após reconexão (se falhou no boot) ou a cada 5 min
  gerenciarSincronizacaoAtributos();

  // 3. Envia telemetria + predição (no boot, a cada 60 s ou imediatamente se bomba/modo mudar)
  verificarEEnviarTelemetria();

  // 4. Envia a hora "T:HH:MM" para a BitDogLab a cada 60 s (ou 5 s até sincronizar o NTP)
  gerenciarEnvioHora();
}
