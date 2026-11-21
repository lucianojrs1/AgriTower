#pragma once

// =========================================================================
// MODELO DE ÁRVORE DE DECISÃO (ALFACE) - exportado do treinamento
// Entradas: x[0]=Temperatura (°C), x[1]=Umidade (%), x[2]=TDS (ppm),
//           x[3]=pH, x[4]=Dias de crescimento
// Saída:    0 = Crescendo, 1 = Pronto para colher, 2 = Estragado
// Para atualizar o modelo, só este arquivo precisa ser substituído.
// =========================================================================
namespace Eloquent {
  namespace ML {
    namespace Port {
      class DecisionTree {
        public:
          float confianca = 0;

          int predict(float *x) {
            if (x[4] <= 34.500000f) {
                if (x[0] <= 32.850000f) {
                    confianca = 1.0000f;
                    return 0;
                } else {
                    if (x[3] <= 6.150000f) {
                        confianca = 1.0000f;
                        return 2;
                    } else {
                        if (x[3] <= 6.650000f) {
                            confianca = 1.0000f;
                            return 0;
                        } else {
                            confianca = 1.0000f;
                            return 2;
                        }
                    }
                }
            } else {
                if (x[4] <= 42.500000f) {
                    confianca = 1.0000f;
                    return 1;
                } else {
                    confianca = 1.0000f;
                    return 2;
                }
            }
          }
      };
    }
  }
}
