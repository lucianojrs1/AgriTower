#include "tb_atributos.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "plantio.h"

static unsigned long ultimaSincronizacaoAtributos = 0;
static bool          atributosSincronizadosNoBoot = false;

void iniciarSincronizacaoAtributos() {
  ultimaSincronizacaoAtributos = millis();
}

bool sincronizarAtributosDashboard() {
  if (WiFi.status() != WL_CONNECTED) return false;

  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  if (!http.begin(cliente, urlAtributos)) {
    Serial.println("[HTTP GET ATRIBUTOS] Falha ao iniciar conexão.");
    return false;
  }

  int resultado = http.GET();
  bool sucesso  = false;

  if (resultado == 200) {
    String resposta = http.getString();
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, resposta);

    if (!error) {
      JsonVariantConst dataPlantio = doc["shared"]["data_plantio"].as<JsonVariantConst>();
      if (!dataPlantio.isNull()) {
        atualizarDataPlantio(dataPlantio);
      }
      sucesso = true;
    } else {
      Serial.print("[ERRO JSON ATRIBUTOS] ");
      Serial.println(error.f_str());
    }
  } else {
    Serial.printf("[HTTP GET ATRIBUTOS] Erro: %d\n", resultado);
  }

  http.end();
  return sucesso;
}

void sincronizarAtributosNoBoot() {
  atributosSincronizadosNoBoot = sincronizarAtributosDashboard();
  ultimaSincronizacaoAtributos = millis();
}

void gerenciarSincronizacaoAtributos() {
  // Imediatamente após reconexão (se falhou no boot) ou a cada intervaloAtributos
  if (WiFi.status() != WL_CONNECTED) return;

  if (!atributosSincronizadosNoBoot ||
      (millis() - ultimaSincronizacaoAtributos >= intervaloAtributos)) {
    ultimaSincronizacaoAtributos = millis();
    if (sincronizarAtributosDashboard()) {
      atributosSincronizadosNoBoot = true;
    }
  }
}
