#pragma once
#include <Arduino.h>

extern const char* const NOMES_STATUS_ALFACE[3];  // Crescendo / Pronto para colher / Estragado

struct DiagnosticoAlface {
  bool   valido       = false;
  int    status       = -1;
  float  confianca    = 0;
  int    dias         = -1;
  bool   confiavel    = true;
  bool   extrapolando = false;
  int    numAlertas   = 0;
  String conclusao;
};

// Roda a árvore de decisão e compara cada sensor com o dataset de referência
DiagnosticoAlface analisarAlface(float temperatura, float umidade, float tds, float ph);

void imprimirDiagnostico(const DiagnosticoAlface& d);
