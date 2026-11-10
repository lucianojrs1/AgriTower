#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

// Inclusão dos módulos da AgriTower
#include "sensores.h"
#include "relogio.h"
#include "botao.h"
#include "agendamento.h"
#include "bomba.h"
#include "comunicacao.h"
#include "oled.h"

void setup() {
    Serial.begin(9600);
    delay(1500);

    Serial.println("\n=======================================================");
    Serial.println("         AGRITOWER - RTOS BITDOGLAB (PICO)             ");
    Serial.println("=======================================================");

    // 1. Inicializa o hardware e os Mutexes ANTES de dar partida nas Tasks
    initRelogio();     // Inicializa Serial2 (GP8/GP9) e Mutex do relógio
    initBotao();       // Inicializa Botão A (GP5), LED Vermelho (GP13) e Mutex
    initAgendamento(); // Inicializa o Mutex do agendador de ciclos
    initBomba();       // Inicializa PWM Bomba (GP4), LED Verde (GP11) e Mutex
    initSensores();    // Inicializa I2C0, ADS1115, DS18B20, DHT22 e Mutex
    initComunicacao();
    initOled(); // Inicializa LED Azul de telemetria (GP12)

    // 2. Criação das Tasks (Rate Monotonic: menor período = maior prioridade)
    // Stacks de 1024 a 2048 words garantem folga total para Serial.print e floats no RP2040
    xTaskCreate(vTaskBomba,       "TaskBomba",       1024, NULL, 3, NULL); // Prio 4 (Máxima - 15ms)
    xTaskCreate(vTaskBotao,       "TaskBotao",       1024, NULL, 3, NULL); // Prio 3 (Alta   - 20ms)
    xTaskCreate(vTaskRelogio,     "TaskRelogio",     1024, NULL, 2, NULL); // Prio 2 (Média  - 100ms)
    xTaskCreate(vTaskAgendamento, "TaskAgendamento", 1024, NULL, 2, NULL); // Prio 2 (Média  - 500ms)
    xTaskCreate(vTaskSensores,    "TaskSensores",    2048, NULL, 2, NULL); // Prio 1 (Baixa  - 2000ms)
    xTaskCreate(vTaskComunicacao, "TaskCom",         2048, NULL, 1, NULL); // Prio 1 (Baixa  - 2000ms)
    xTaskCreate(vTaskOled,    "TaskOled",    2048, NULL, 1, NULL); // Prio 1 (Baixa  - 2000ms)


    Serial.println("[SISTEMA] Todas as Tasks RTOS iniciadas com sucesso!");
    Serial.println("=======================================================\n");
}

void loop() {
    // No core earlephilhower (RP2040), o loop() roda como uma task de baixa prioridade.
    // Deixamos apenas um delay longo para liberar 100% da CPU para as nossas 6 Tasks!
    vTaskDelay(pdMS_TO_TICKS(5000));
}