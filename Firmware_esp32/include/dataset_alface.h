#pragma once

// Estatísticas de referência do dataset de treino (alface)
constexpr int NUM_SENSORES_REF = 4;   // Temperatura, Umidade, TDS, pH
constexpr int REF_DIA_MIN      = 1;
constexpr int REF_DIA_MAX      = 48;

extern const char* const NOMES_SENSORES_REF[NUM_SENSORES_REF];
extern const char* const UNIDADES_REF[NUM_SENSORES_REF];

extern const float REF_MIN[NUM_SENSORES_REF];
extern const float REF_MAX[NUM_SENSORES_REF];
extern const float REF_P5[NUM_SENSORES_REF];
extern const float REF_P95[NUM_SENSORES_REF];

// Média e desvio padrão por dia de crescimento (índice 0 = dia 1)
extern const float REF_MEDIA_DIA[REF_DIA_MAX][NUM_SENSORES_REF];
extern const float REF_DP_DIA[REF_DIA_MAX][NUM_SENSORES_REF];
