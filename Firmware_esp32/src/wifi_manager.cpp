#include "wifi_manager.h"
#include <Arduino.h>
#include <WiFi.h>
#include "config.h"
#include "leds.h"

static unsigned long ultimaTentativaWifi = 0;

bool iniciarWifi() {
  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.begin(ssid, senha);
  Serial.print("[WIFI] Conectando a ");
  Serial.println(ssid);

  // Aguarda até timeoutWifiBoot no boot; se não conectar, segue operando e reconecta no loop()
  unsigned long inicioWifi = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - inicioWifi < timeoutWifiBoot)) {
    atualizarLeds();
    delay(100);
    Serial.print(".");
  }
  atualizarLeds();

  ultimaTentativaWifi = millis();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WIFI] Conectado! IP: " + WiFi.localIP().toString());
    return true;
  }
  Serial.println("\n[WIFI] Timeout no boot. Reconexão automática seguirá em segundo plano.");
  return false;
}

void gerenciarConexaoWifi() {
  if (WiFi.status() == WL_CONNECTED) return;

  if (millis() - ultimaTentativaWifi >= intervaloReconexaoWifi) {
    ultimaTentativaWifi = millis();
    Serial.println("[WIFI] Sem conexão. Tentando reconectar...");
    WiFi.disconnect();
    WiFi.begin(ssid, senha);
  }
}
