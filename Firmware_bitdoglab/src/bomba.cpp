#include "bomba.h"
#include "botao.h"
#include "agendamento.h"

// --- Mapeamento de Pinos de Atuação ---
static const int pinBomba  = 4;  // Bomba 12V via Relé (Active LOW: LOW = Liga, HIGH = Desliga)
static const int pinLedG   = 11; // LED Verde da BitDogLab (Bomba Ligada)
static const int pinBuzzer = 10; // Buzzer B da BitDogLab (Aviso sonoro de partida)

// --- Variáveis Internas Protegidas por Mutex ---
static EstadoBomba estadoAtualBomba = DESLIGADA;
static SemaphoreHandle_t xMutexBomba = NULL;

// =========================================================================
// Função auxiliar de texto para o Painel Serial
// =========================================================================
const char* nomeEstadoBomba(EstadoBomba est) {
    switch (est) {
        case DESLIGADA:  return "DESLIGADA (OFF)";
        case LIGANDO:    return "ACIONANDO RELE...";
        case LIGADA:     return "LIGADA (ON)";
        case DESLIGANDO: return "DESLIGANDO RELE...";
        default:         return "DESCONHECIDO";
    }
}

// =========================================================================
// Inicialização do Hardware e do Mutex (Chamar no setup() do main.cpp)
// =========================================================================
void initBomba() {
    xMutexBomba = xSemaphoreCreateMutex();

    pinMode(pinBomba, OUTPUT);
    pinMode(pinLedG, OUTPUT);
    pinMode(pinBuzzer, OUTPUT);

    // IMPORTANTE: Relé é Active LOW, portanto inicia em HIGH para começar DESLIGADO!
    digitalWrite(pinBomba, HIGH);
    digitalWrite(pinLedG, LOW);
    digitalWrite(pinBuzzer, LOW);

    Serial.println("[INIT BOMBA] Rele (GP4 em HIGH/OFF), LED Verde (GP11) e Buzzer (GP10) configurados.");
}

// =========================================================================
// Função Thread-Safe para a Telemetria e OLED consultarem o Estado
// =========================================================================
StatusBomba_t obterStatusBomba() {
    StatusBomba_t copiaSegura = {DESLIGADA}; // Inicializa apenas o campo 'estado'

    if (xMutexBomba != NULL) {
        if (xSemaphoreTake(xMutexBomba, pdMS_TO_TICKS(20)) == pdTRUE) {
            copiaSegura.estado = estadoAtualBomba;
            xSemaphoreGive(xMutexBomba);
        }
    }
    return copiaSegura;
}

// =========================================================================
// TASK RTOS: Controle de Acionamento ON/OFF do Relé
// =========================================================================
void vTaskBomba(void *pvParameters) {
    (void) pvParameters;

    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xIntervalo = pdMS_TO_TICKS(50); // 50ms: resposta rápida ao botão sem gastar CPU

    EstadoBomba estadoLocal = DESLIGADA;

    for (;;) {
        // -----------------------------------------------------------------
        // 1. LÓGICA DE TRANSIÇÃO DE ESTADOS (Obedece ao Modo Auto ou Manual)
        // -----------------------------------------------------------------
        if (isModoAutomatico()) {
            StatusAgendamento_t agenda = obterStatusAgendamento();

            if (agenda.deveEstarLigada) {
                if (estadoLocal == DESLIGADA || estadoLocal == DESLIGANDO) {
                    estadoLocal = LIGANDO;
                }
            } else {
                if (estadoLocal == LIGADA || estadoLocal == LIGANDO) {
                    estadoLocal = DESLIGANDO;
                }
            }
        } else {
            // Em Modo Manual, verifica se o Botão A solicitou inversão
            if (consumirPedidoInversaoManual()) {
                if (estadoLocal == DESLIGADA || estadoLocal == DESLIGANDO) {
                    estadoLocal = LIGANDO;
                    Serial.println("[BOMBA] Override Manual -> Ligando Bomba via Rele!");
                } else {
                    estadoLocal = DESLIGANDO;
                    Serial.println("[BOMBA] Override Manual -> Desligando Bomba via Rele!");
                }
            }
        }

        // -----------------------------------------------------------------
        // 2. EXECUÇÃO FÍSICA NO RELÉ (Active LOW), LED VERDE E BUZZER
        // -----------------------------------------------------------------
        switch (estadoLocal) {
            case LIGANDO:
                digitalWrite(pinBomba, LOW);     // LOW atraca o módulo relé
                digitalWrite(pinLedG, HIGH);     // Acende LED Verde da BitDogLab
                tone(pinBuzzer, 2500, 150);      // Bipe de 150ms no GP10 (não-bloqueante)
                estadoLocal = LIGADA;
                Serial.println("[BOMBA] Rele Acionado (ON)");
                break;

            case DESLIGANDO:
                digitalWrite(pinBomba, HIGH);    // HIGH solta o módulo relé
                digitalWrite(pinLedG, LOW);      // Apaga LED Verde
                estadoLocal = DESLIGADA;
                Serial.println("[BOMBA] Rele Desligado (OFF)");
                break;

            case LIGADA:
            case DESLIGADA:
                break;
        }

        // -----------------------------------------------------------------
        // 3. ATUALIZA VARIÁVEL GLOBAL PROTEGIDA POR MUTEX
        // -----------------------------------------------------------------
        if (xMutexBomba != NULL) {
            if (xSemaphoreTake(xMutexBomba, 0) == pdTRUE) {
                estadoAtualBomba = estadoLocal;
                xSemaphoreGive(xMutexBomba);
            }
        }

        vTaskDelayUntil(&xLastWakeTime, xIntervalo);
    }
}