#include "modelo_alface.h"
#include "arvore_decisao.h"
#include "dataset_alface.h"
#include "plantio.h"
#include "config.h"

static Eloquent::ML::Port::DecisionTree modeloAlface;

const char* const NOMES_STATUS_ALFACE[3] = {"Crescendo", "Pronto para colher", "Estragado"};

static const char* const DICA_ALTA[NUM_SENSORES_REF] = {
  "ventilar/sombrear a torre",
  "melhorar a ventilação (risco de fungos)",
  "diluir a solução com água",
  "baixar o pH da solução"
};
static const char* const DICA_BAIXA[NUM_SENSORES_REF] = {
  "proteger a torre do frio",
  "ar seco, a planta perde mais água",
  "repor a solução nutritiva",
  "subir o pH da solução"
};

DiagnosticoAlface analisarAlface(float temperatura, float umidade, float tds, float ph) {
  DiagnosticoAlface d;
  d.dias = calcularDiasCrescimento();

  if (d.dias == DIAS_SEM_RELOGIO) {
    d.conclusao = "Relógio sem NTP: não dá para calcular os dias de crescimento.";
    return d;
  }
  if (d.dias < 1) {
    d.conclusao = "Data de plantio depois de hoje: confira o campo data_plantio no dashboard.";
    return d;
  }
  d.valido = true;

  float x[5] = {temperatura, umidade, tds, ph, (float)d.dias};
  d.status    = modeloAlface.predict(x);
  d.confianca = modeloAlface.confianca;

  d.extrapolando = (d.dias < REF_DIA_MIN || d.dias > REF_DIA_MAX);
  int idxDia = constrain(d.dias, REF_DIA_MIN, REF_DIA_MAX) - REF_DIA_MIN;

  String falhas  = "";
  String alertas = "";

  for (int i = 0; i < NUM_SENSORES_REF; i++) {
    float v = x[i];
    if (isnan(v) || v == 0) {
      if (falhas.length() > 0) falhas += ", ";
      falhas += NOMES_SENSORES_REF[i];
      continue;
    }

    float media = REF_MEDIA_DIA[idxDia][i];
    float dp    = REF_DP_DIA[idxDia][i];
    float z     = (dp > 0) ? (v - media) / dp : 0;

    const char* situacao = nullptr;
    bool alto = false;

    if (v < REF_MIN[i])      { situacao = "abaixo de tudo no dataset"; d.extrapolando = true; }
    else if (v > REF_MAX[i]) { situacao = "acima de tudo no dataset";  alto = true; d.extrapolando = true; }
    else if (v < REF_P5[i])  { situacao = "abaixo da faixa das plantas saudáveis"; }
    else if (v > REF_P95[i]) { situacao = "acima da faixa das plantas saudáveis"; alto = true; }
    else if (fabs(z) > 2)    { situacao = "incomum para plantas nessa fase"; alto = (z > 0); }

    if (situacao != nullptr) {
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

  d.confiavel = (falhas.length() == 0);
  if (!d.confiavel) {
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

void imprimirDiagnostico(const DiagnosticoAlface& d) {
  Serial.print("[MODELO] ");
  if (d.valido) {
    Serial.print("Dia ");
    Serial.print(d.dias);
    Serial.print(" -> ");
  }
  Serial.println(d.conclusao);
}
