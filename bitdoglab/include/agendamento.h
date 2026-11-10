#ifndef AGENDAMENTO_H
#define AGENDAMENTO_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Estrutura com informações do ciclo atual (útil para a Bomba e para o Painel Serial)
typedef struct {
    bool deveEstarLigada; // true = janela de 5 min ON | false = janela de pausa OFF
    bool ehDia;           // true = 06:00 às 17:59 (ciclo 15m) | false = noite (ciclo 50m)
    int minutoNoCiclo;    // Ex: minuto 3 de 15
    int tamanhoCiclo;     // 15 (dia) ou 50 (noite)
} StatusAgendamento_t;

// Inicializa o Mutex do módulo de agendamento
void initAgendamento();

// Protótipo da Task do FreeRTOS (chamar no xTaskCreate do main.cpp)
void vTaskAgendamento(void *pvParameters);

// Função Thread-Safe para a Bomba e a Telemetria consultarem a decisão do agendador
StatusAgendamento_t obterStatusAgendamento();

#endif // AGENDAMENTO_H