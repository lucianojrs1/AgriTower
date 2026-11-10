#include "comunicacao.h"
#include "sensores.h"
#include "relogio.h"
#include "botao.h"
#include "agendamento.h"
#include "bomba.h"

// --- Mapeamento do Pino de Status de Telemetria ---
static const int pinLedB = 12; // LED Azul da BitDogLab (Pulso de Telemetria)

// =========================================================================
// Inicialização do Hardware (Chamar no setup() do main.cpp)
// =========================================================================
void initComunicacao() {
    pinMode(pinLedB, OUTPUT);
    digitalWrite(pinLedB, LOW);

    Serial.println("[INIT COMUNICACAO] Telemetria ESP32 e Painel Serial configurados (LED Azul GP12).");
}

// =========================================================================
// TASK RTOS: Envio de Telemetria (Serial2) e Painel USB (A cada 500ms)
// =========================================================================
void vTaskComunicacao(void *pvParameters) {
    (void) pvParameters;

    // Aguarda 1000ms iniciais para a vTaskSensores concluir a 1ª leitura do DS18B20/DHT22
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Controle de periodicidade exata de 2000ms (2 segundos)
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequencia = pdMS_TO_TICKS(500); //MUDEI PARA MEIO SEGUNDO A FREQ

    for (;;) {
        // Acende o LED Azul indicando início da transmissão de telemetria
        digitalWrite(pinLedB, HIGH);

        // -----------------------------------------------------------------
        // 0. Coleta Thread-Safe (via Mutex) de todos os módulos do sistema
        // -----------------------------------------------------------------
        DadosSensores_t     sens   = obterDadosSensores();
        Horario_t           relog  = obterHorario();
        StatusAgendamento_t agenda = obterStatusAgendamento();
        StatusBomba_t       bomba  = obterStatusBomba();
        bool                autoOn = isModoAutomatico();

        // -----------------------------------------------------------------
        // 1. Envia pacote compacto via Serial2 para o ESP32 (ThingsBoard)
        // -----------------------------------------------------------------
        Serial2.print("B:");   Serial2.print(sens.tensaoBateria, 1);
        Serial2.print(",L:");  Serial2.print(sens.luminosidade);
        Serial2.print(",TA:"); Serial2.print(sens.tempAgua, 1);
        Serial2.print(",TR:"); Serial2.print(sens.tempAr, 1);
        Serial2.print(",U:");  Serial2.print(sens.umidade, 0);
        Serial2.print(",P:");  Serial2.print(sens.valorPH, 2);
        Serial2.print(",S:");  Serial2.print(sens.valorTDS, 0);
        Serial2.print(",M:");  Serial2.print(bomba.pwmAtual);
        Serial2.print(",A:");  Serial2.print(autoOn ? 1 : 0);
        Serial2.print(",T:");
        if (relog.hora < 10) Serial2.print("0");
        Serial2.print(relog.hora);
        Serial2.print(":");
        if (relog.minuto < 10) Serial2.print("0");
        Serial2.println(relog.minuto);

        // -----------------------------------------------------------------
        // 2. Imprime Painel Completo no Monitor Série do PC (USB)
        // -----------------------------------------------------------------
        Serial.println("-----------------------------------------------------------------------");
        Serial.print("[STATUS] Hora: ");
        if (relog.hora < 10) Serial.print("0");
        Serial.print(relog.hora);
        Serial.print(":");
        if (relog.minuto < 10) Serial.print("0");
        Serial.print(relog.minuto);
        Serial.print(agenda.ehDia ? " (DIA - Ciclo 5m/10m)" : " (NOITE - Ciclo 5m/45m)");
        Serial.print(" | Modo: ");
        Serial.print(autoOn ? "AUTOMATICO" : "MANUAL (Botao A)");
        Serial.print(" | Minuto do Ciclo: ");
        Serial.print(agenda.minutoNoCiclo);
        Serial.print("/");
        Serial.println(agenda.tamanhoCiclo);

        Serial.print("[BOMBA]  Estado: ");
        Serial.print(nomeEstadoBomba(bomba.estado));
        Serial.print(" | PWM Atual: ");
        Serial.print(bomba.pwmAtual);
        Serial.println("/255");

        Serial.print("[SENSORES] Bat: "); Serial.print(sens.tensaoBateria, 1);
        Serial.print("V | Luz: ");        Serial.print(sens.luminosidade);
        Serial.print("% | T_Agua: ");     Serial.print(sens.tempAgua, 1);
        Serial.print("C | T_Ar: ");       Serial.print(sens.tempAr, 1);
        Serial.print("C | Umid: ");       Serial.print(sens.umidade, 0);
        Serial.print("% | pH: ");         Serial.print(sens.valorPH, 2);
        Serial.print(" | TDS: ");         Serial.print(sens.valorTDS, 0);
        Serial.println(" ppm");

        // Mantém o LED Azul aceso por 50ms para o pulso ficar visível a olho nu
        vTaskDelay(pdMS_TO_TICKS(50));
        digitalWrite(pinLedB, LOW);

        // Dorme até completar o período exato de 2000ms
        vTaskDelayUntil(&xLastWakeTime, xFrequencia);
    }
}