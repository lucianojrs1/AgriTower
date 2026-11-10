#include "bomba.h"
#include "botao.h"
#include "agendamento.h"

// --- Mapeamento de Pinos de Atuação ---
static const int pinBomba = 4;  // Bomba 12V via TIP120 (IDC 8)
static const int pinLedG  = 11; // LED Verde da BitDogLab (Bomba em 100%)

// --- Parâmetros da Rampa PWM ---
static const int passoFade = 5; // 255 / 5 = 51 passos de 15ms (~765ms de rampa)

// --- Variáveis Internas Protegidas por Mutex ---
static EstadoBomba estadoAtualBomba = DESLIGADA;
static int pwmAtual = 0;
static SemaphoreHandle_t xMutexBomba = NULL;

// =========================================================================
// Função auxiliar de texto para o Painel Serial
// =========================================================================
const char* nomeEstadoBomba(EstadoBomba est) {
    switch (est) {
        case DESLIGADA:  return "DESLIGADA (0%)";
        case LIGANDO:    return "SUBINDO PWM...";
        case LIGADA:     return "LIGADA (100%)";
        case DESLIGANDO: return "DESCENDO PWM...";
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

    analogWrite(pinBomba, 0);
    digitalWrite(pinLedG, LOW);

    Serial.println("[INIT BOMBA] Pino PWM (GP4) e LED Verde (GP11) configurados.");
}

// =========================================================================
// Função Thread-Safe para a Telemetria consultar o PWM e o Estado
// =========================================================================
StatusBomba_t obterStatusBomba() {
    StatusBomba_t copiaSegura = {DESLIGADA, 0};

    if (xMutexBomba != NULL) {
        if (xSemaphoreTake(xMutexBomba, pdMS_TO_TICKS(20)) == pdTRUE) {
            copiaSegura.estado   = estadoAtualBomba;
            copiaSegura.pwmAtual = pwmAtual;
            xSemaphoreGive(xMutexBomba);
        }
    }
    return copiaSegura;
}

// =========================================================================
// TASK RTOS: Controle de Transição e Rampa PWM Suave (A cada 15ms exatos)
// =========================================================================
void vTaskBomba(void *pvParameters) {
    (void) pvParameters;

    // Variáveis para garantir periodicidade estrita de 15ms sem jitter
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xIntervaloFade = pdMS_TO_TICKS(15);

    for (;;) {
        // -----------------------------------------------------------------
        // 1. LÓGICA DE TRANSIÇÃO DE ESTADOS (Obedece ao Modo Auto ou Manual)
        // -----------------------------------------------------------------
        if (isModoAutomatico()) {
            // Em Modo Automático, segue a decisão do Agendador Horário
            StatusAgendamento_t agenda = obterStatusAgendamento();

            if (agenda.deveEstarLigada) {
                if (estadoAtualBomba == DESLIGADA || estadoAtualBomba == DESLIGANDO) {
                    estadoAtualBomba = LIGANDO;
                }
            } else {
                if (estadoAtualBomba == LIGADA || estadoAtualBomba == LIGANDO) {
                    estadoAtualBomba = DESLIGANDO;
                }
            }
        } else {
            // Em Modo Manual, verifica se o Botão A solicitou inversão
            if (consumirPedidoInversaoManual()) {
                if (estadoAtualBomba == DESLIGADA || estadoAtualBomba == DESLIGANDO) {
                    estadoAtualBomba = LIGANDO;
                    Serial.println("[BOMBA] Override Manual -> Iniciando subida da rampa PWM!");
                } else {
                    estadoAtualBomba = DESLIGANDO;
                    Serial.println("[BOMBA] Override Manual -> Iniciando descida da rampa PWM!");
                }
            }
        }

        // -----------------------------------------------------------------
        // 2. EXECUÇÃO FÍSICA DA RAMPA PWM SUAVE (Passo de +/- 5 a cada 15ms)
        // -----------------------------------------------------------------
        switch (estadoAtualBomba) {
            case LIGANDO:
                pwmAtual += passoFade;
                if (pwmAtual >= 255) {
                    pwmAtual = 255;
                    estadoAtualBomba = LIGADA;
                    digitalWrite(pinLedG, HIGH);
                    Serial.println(">>> [PWM BOMBA] Rampa concluida: Bomba em 100% (PWM = 255).");
                }
                analogWrite(pinBomba, pwmAtual);
                break;

            case DESLIGANDO:
                pwmAtual -= passoFade;
                if (pwmAtual <= 0) {
                    pwmAtual = 0;
                    estadoAtualBomba = DESLIGADA;
                    digitalWrite(pinLedG, LOW);
                    Serial.println(">>> [PWM BOMBA] Rampa concluida: Bomba totalmente DESLIGADA (PWM = 0).");
                }
                analogWrite(pinBomba, pwmAtual);
                break;

            case LIGADA:
            case DESLIGADA:
                // Mantém o estado estável sem gastar escrita desnecessária no hardware
                break;
        }

        // -----------------------------------------------------------------
        // 3. ATUALIZA DADOS COMPARTILHADOS COM A TELEMETRIA (Mutex rápido)
        // -----------------------------------------------------------------
        // Como só vTaskBomba escreve em pwmAtual e estadoAtualBomba, protegemos
        // o acesso para garantir leitura atômica pela Task de Telemetria.
        if (xMutexBomba != NULL) {
            if (xSemaphoreTake(xMutexBomba, 0) == pdTRUE) { // Timeout 0 para nunca atrasar a rampa!
                xSemaphoreGive(xMutexBomba);
            }
        }

        // Dorme até fechar exatamente 15ms desde o início deste ciclo
        vTaskDelayUntil(&xLastWakeTime, xIntervaloFade);
    }
}