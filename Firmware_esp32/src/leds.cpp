#include "leds.h"
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"

static unsigned long ledSerialDesligaEm = 0;
static unsigned long ultimaPiscadaWifi  = 0;
static bool          estadoLedWifi      = false;

void iniciarLeds() {
  pinMode(LED_WIFI, OUTPUT);
  pinMode(LED_SERIAL, OUTPUT);
  digitalWrite(LED_WIFI, LOW);
  digitalWrite(LED_SERIAL, LOW);
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
