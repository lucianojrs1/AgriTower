#include "agendamento.h"
#include "relogio.h"
#include "botao.h"

// --- Variáveis Privadas do Módulo (Encapsuladas com static) ---
static StatusAgendamento_t statusAtual = {false, true, 0, 15};
static SemaphoreHandle_t xMutexAgendamento = NULL;

// =========================================================================
// Inicialização do Mutex (Chamar no setup() do main.cpp)
// =========================================================================
void initAgendamento() {
    xMutexAgendamento = xSemaphoreCreateMutex();
    Serial.println("[INIT AGENDAMENTO] Modulo do Agendador Horario inicializado.");
}

// =========================================================================
// Função Thread-Safe para outras Tasks consultarem o Agendador
// =========================================================================
StatusAgendamento_t obterStatusAgendamento() {
    StatusAgendamento_t copiaSegura = {false, true, 0, 15};

    if (xMutexAgendamento != NULL) {
        if (xSemaphoreTake(xMutexAgendamento, pdMS_TO_TICKS(50)) == pdTRUE) {
            copiaSegura = statusAtual;
            xSemaphoreGive(xMutexAgendamento);
        }
    }
    return copiaSegura;
}

// =========================================================================
// TASK RTOS: Avaliação da Janela de Irrigação (A cada 500ms)
// =========================================================================
void vTaskAgendamento(void *pvParameters) {
    (void) pvParameters;

    // Guarda o estado anterior para imprimir no Serial apenas quando houver mudança
    bool deviaEstarLigadaAnterior = false;
    bool primeiraExecucao = true;

    for (;;) {
        // 1. Puxa a hora atual de forma segura do módulo relogio.h
        Horario_t agora = obterHorario();

        // 2. Calcula os parâmetros do ciclo (Diurno vs Noturno)
        int minutoDoDia = (agora.hora * 60) + agora.minuto;
        bool ehDia = (agora.hora >= 6 && agora.hora < 18); // Dia: 06:00 às 17:59
        bool deveEstarLigada = false;
        int tamanhoCiclo = ehDia ? 15 : 50;
        int minutoNoCiclo = minutoDoDia % tamanhoCiclo;

        if (ehDia) {
            // Ciclo Diurno de 15 min (5 min ON / 10 min OFF)
            deveEstarLigada = (minutoNoCiclo < 5);
        } else {
            // Ciclo Noturno de 50 min (5 min ON / 45 min OFF)
            deveEstarLigada = (minutoNoCiclo < 5);
        }

        // 3. SEÇÃO CRÍTICA: Atualiza a estrutura protegida pelo Mutex
        if (xMutexAgendamento != NULL) {
            if (xSemaphoreTake(xMutexAgendamento, pdMS_TO_TICKS(100)) == pdTRUE) {
                statusAtual.deveEstarLigada = deveEstarLigada;
                statusAtual.ehDia           = ehDia;
                statusAtual.minutoNoCiclo   = minutoNoCiclo;
                statusAtual.tamanhoCiclo    = tamanhoCiclo;
                xSemaphoreGive(xMutexAgendamento);
            }
        }

        // 4. Se estiver em Modo Automático, exibe o log quando a janela virar
        if (isModoAutomatico()) {
            if (primeiraExecucao || (deveEstarLigada != deviaEstarLigadaAnterior)) {
                primeiraExecucao = false;
                deviaEstarLigadaAnterior = deveEstarLigada;

                if (deveEstarLigada) {
                    Serial.println("\n>>> [AUTO AGENDADOR] Janela de irrigacao iniciada (5 min)! Solicitando subida PWM...");
                } else {
                    Serial.println("\n>>> [AUTO AGENDADOR] Janela de pausa iniciada! Solicitando descida PWM...");
                }
            }
        } else {
            // Se o usuário colocou em Modo Manual, força reavaliação do log quando voltar pro Auto
            primeiraExecucao = true;
        }

        // Dorme por 500ms (2 checagens por segundo é ideal para um relógio de minutos)
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}