#include "tb_telemetria.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include "config.h"
#include "serial_pico.h"
#include "modelo_alface.h"
#include "plantio.h"

static unsigned long ultimoPostTelemetria     = 0;
static unsigned long intervaloPostAtual       = intervaloPostTb;
static bool          primeiroPostFeito        = false;

// Rastreamento de mudança de estado (envio imediato)
static int ultimoEstadoBombaEnviado = -1;
static int ultimoModoAutoEnviado    = -1;

// Monta o JSON no formato enxuto do dashboard
static String montarPayload(const DiagnosticoAlface& diag) {
  JsonDocument pacote;
  pacote["luminosidade"]  = ultimosDadosPico.luminosidade;
  pacote["temp_agua"]     = ultimosDadosPico.tempAgua;
  pacote["temp_ar"]       = ultimosDadosPico.tempAr;
  pacote["umidade"]       = ultimosDadosPico.umidade;
  pacote["nivel_bateria"] = ultimosDadosPico.tensaoBat;
  pacote["ph"]            = ultimosDadosPico.ph;
  pacote["ec"]            = ultimosDadosPico.tds;

  if (ultimosDadosPico.temEstadoECiclo) {
    char ciclo_str[16];
    snprintf(ciclo_str, sizeof(ciclo_str), "%d/%d",
             ultimosDadosPico.minCiclo, ultimosDadosPico.tamCiclo);

    pacote["bomba_ativa"] = (ultimosDadosPico.estadoBomba == 1);
    pacote["ciclo_bomba"] = ciclo_str;
  }

  pacote["modo_automatico"] = (ultimosDadosPico.modoAuto == 1);

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
  formatarDataPlantio(plantio_str, sizeof(plantio_str));
  pacote["data_plantio_em_uso"] = plantio_str;
  pacote["data_plantio_origem"] = dataPlantioVeioDoDashboard() ? "dashboard" : "código";

  String payload;
  serializeJson(pacote, payload);
  return payload;
}

// Retorna false se não conseguiu nem iniciar a conexão; caso contrário, codigoHttp tem o retorno do POST
static bool postarPayload(const String& payload, int& codigoHttp) {
  WiFiClientSecure cliente;
  cliente.setInsecure();
  HTTPClient http;
  http.setConnectTimeout(5000);
  http.setTimeout(5000);

  if (!http.begin(cliente, server)) {
    Serial.println("[THINGSBOARD] Falha ao iniciar conexão HTTPS.");
    return false;
  }

  http.addHeader("Content-Type", "application/json");
  codigoHttp = http.POST(payload);
  http.end();
  return true;
}

void verificarEEnviarTelemetria() {
  if (!ultimosDadosPico.disponivel) return;

  unsigned long agora = millis();

  // Detecta mudança imediata no estado do relé ou no modo Auto/Manual
  bool mudouBomba = ultimosDadosPico.temEstadoECiclo &&
                    (ultimosDadosPico.estadoBomba != ultimoEstadoBombaEnviado);
  bool mudouModo  = (ultimosDadosPico.modoAuto != ultimoModoAutoEnviado);
  bool mudancaCritica = primeiroPostFeito && (mudouBomba || mudouModo) &&
                        (agora - ultimoPostTelemetria >= intervaloPostEventoMinimo);

  bool horaDoEnvioPeriodico = !primeiroPostFeito ||
                              (agora - ultimoPostTelemetria >= intervaloPostAtual);

  if (!horaDoEnvioPeriodico && !mudancaCritica) return;

  Serial.print("\n[PICO RX] ");
  Serial.println(ultimosDadosPico.pacoteBruto);
  if (mudancaCritica) {
    Serial.println("[EVENTO] Mudança de estado da bomba/modo detectada -> envio imediato!");
  }

  // Executa o modelo de Árvore de Decisão localmente (funciona mesmo sem Wi-Fi)
  DiagnosticoAlface diag = analisarAlface(ultimosDadosPico.tempAr,
                                          (float)ultimosDadosPico.umidade,
                                          (float)ultimosDadosPico.tds,
                                          ultimosDadosPico.ph);
  imprimirDiagnostico(diag);

  if (WiFi.status() != WL_CONNECTED) {
    // Sem Wi-Fi, tenta novamente em 10 s assim que a conexão voltar
    ultimoPostTelemetria = agora;
    intervaloPostAtual   = intervaloRetryHttp;
    return;
  }

  String payload = montarPayload(diag);

  int codigoHttp = 0;
  if (!postarPayload(payload, codigoHttp)) {
    ultimoPostTelemetria = millis();
    intervaloPostAtual   = intervaloRetryHttp;
    return;
  }

  ultimoPostTelemetria = millis();

  if (codigoHttp == 200) {
    Serial.println("[THINGSBOARD] Telemetria + Predição enviadas com sucesso (HTTP 200).");
    primeiroPostFeito     = true;
    intervaloPostAtual    = intervaloPostTb;
    ultimoModoAutoEnviado = ultimosDadosPico.modoAuto;
    if (ultimosDadosPico.temEstadoECiclo) {
      ultimoEstadoBombaEnviado = ultimosDadosPico.estadoBomba;
    }
  } else {
    Serial.printf("[THINGSBOARD] Erro no envio HTTP: %d (nova tentativa em 10 s)\n", codigoHttp);
    intervaloPostAtual = intervaloRetryHttp;
  }
}
