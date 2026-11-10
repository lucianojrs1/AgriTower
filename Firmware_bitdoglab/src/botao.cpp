#include "botao.h"

// --- Mapeamento de Pinos da Interface (BitDogLab) ---
static const int pinoBotaoA = 5;  // Botão A (Alterna Auto / Manual)
static const int pinLedR    = 13; // LED Vermelho (Aceso = Modo Manual)

// --- Variáveis de Estado Protegidas por Mutex ---
static bool modoAutomatico = true;
static bool pedidoInversaoManual = false; // Flag para avisar a bomba
static SemaphoreHandle_t xMutexBotao = NULL;

// =========================================================================
// Inicialização do Hardware e do Mutex (Chamar no setup() do main.cpp)
// =========================================================================
void initBotao() {
    xMutexBotao = xSemaphoreCreateMutex();

    pinMode(pinoBotaoA, INPUT_PULLUP);
    pinMode(pinLedR, OUTPUT);
    digitalWrite(pinLedR, LOW); // Inicia apagado (Modo Automático)

    Serial.println("[INIT BOTAO] Botao A (GP5) e LED Vermelho (GP13) configurados.");
}

// =========================================================================
// Funções Thread-Safe para outras Tasks consultarem o Modo
// =========================================================================
bool isModoAutomatico() {
    bool modo = true;
    if (xMutexBotao != NULL) {
        if (xSemaphoreTake(xMutexBotao, pdMS_TO_TICKS(50)) == pdTRUE) {
            modo = modoAutomatico;
            xSemaphoreGive(xMutexBotao);
        }
    }
    return modo;
}

bool consumirPedidoInversaoManual() {
    bool pedido = false;
    if (xMutexBotao != NULL) {
        if (xSemaphoreTake(xMutexBotao, pdMS_TO_TICKS(50)) == pdTRUE) {
            if (pedidoInversaoManual) {
                pedido = true;
                pedidoInversaoManual = false; // Consome o pedido para executar só 1 vez!
            }
            xSemaphoreGive(xMutexBotao);
        }
    }
    return pedido;
}

// =========================================================================
// TASK RTOS: Leitura do Botão A com Debounce por Amostragem (A cada 20ms)
// =========================================================================
void vTaskBotao(void *pvParameters) {
    (void) pvParameters;

    int estadoEstavel = HIGH;     // Estado confirmado após debounce
    int leituraAnterior = HIGH;   // Leitura da iteração passada (20ms atrás)
    int contadorEstabilidade = 0; // Conta leituras iguais consecutivas

    for (;;) {
        int leituraAtual = digitalRead(pinoBotaoA);

        // Se a leitura atual é igual à anterior, incrementa estabilidade
        if (leituraAtual == leituraAnterior) {
            contadorEstabilidade++;
        } else {
            contadorEstabilidade = 0; // Reset se houve oscilação (bouncing)
        }
        leituraAnterior = leituraAtual;

        // 2 leituras iguais seguidas a cada 20ms = 40ms de sinal estável (Debounce concluído!)
        if (contadorEstabilidade >= 2 && leituraAtual != estadoEstavel) {
            estadoEstavel = leituraAtual;

            // Se a borda estável foi de descida (botão pressionado = LOW)
            if (estadoEstavel == LOW) {
                Serial.println("\n*******************************************************");

                // SEÇÃO CRÍTICA: Altera o modo protegendo com Mutex
                if (xMutexBotao != NULL && xSemaphoreTake(xMutexBotao, pdMS_TO_TICKS(100)) == pdTRUE) {
                    if (modoAutomatico) {
                        modoAutomatico = false;
                        pedidoInversaoManual = true; // Avisa a Task da Bomba para inverter!
                        digitalWrite(pinLedR, HIGH); // Acende LED Vermelho
                        Serial.println("[BOTAO A] MODO MANUAL ATIVADO -> Solicitando inversao da bomba!");
                    } else {
                        modoAutomatico = true;
                        pedidoInversaoManual = false;
                        digitalWrite(pinLedR, LOW);  // Apaga LED Vermelho
                        Serial.println("[BOTAO A] MODO AUTOMATICO REATIVADO -> Controle entregue ao relogio!");
                    }
                    xSemaphoreGive(xMutexBotao);
                }

                Serial.println("*******************************************************\n");
            }
        }

        // Dorme por 20ms (consome quase 0% de CPU e mantém resposta instantânea ao clique)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}