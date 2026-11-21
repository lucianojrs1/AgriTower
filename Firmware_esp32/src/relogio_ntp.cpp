#include "relogio_ntp.h"
#include <WiFi.h>
#include "time.h"
#include "config.h"
#include "serial_pico.h"

static unsigned long ultimoEnvioHora = 0;
static bool          horaEnviada     = false;

void iniciarNtp() {
  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
  ultimoEnvioHora = millis();
}

bool enviarHoraParaPico(uint32_t timeoutMs) {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, timeoutMs)) {
    Serial.println("[NTP] Relógio ainda não sincronizado");
    return false;
  }

  char bufferHora[10];
  snprintf(bufferHora, sizeof(bufferHora), "T:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  enviarLinhaPico(bufferHora);

  Serial.print("[NTP] Hora enviada para a BitDogLab: ");
  Serial.println(bufferHora);
  return true;
}

void sincronizarHoraNoBoot() {
  Serial.println("[NTP] Sincronizando relógio...");
  horaEnviada     = enviarHoraParaPico(5000);
  ultimoEnvioHora = millis();
}

void gerenciarEnvioHora() {
  // A cada 60 s (ou a cada 5 s até sincronizar o NTP)
  unsigned long intervaloHora = horaEnviada ? intervaloEnvioHora : intervaloRetentativaHora;
  if (WiFi.status() == WL_CONNECTED && (millis() - ultimoEnvioHora >= intervaloHora)) {
    ultimoEnvioHora = millis();
    horaEnviada = enviarHoraParaPico(200);
  }
}
