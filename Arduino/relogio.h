#ifndef RELOGIO_H
#define RELOGIO_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Estrutura que guarda o horário atual do sistema
typedef struct {
    int hora;
    int minuto;
    bool sincronizadoEsp; // Indica se já recebeu hora do ESP32 pelo menos uma vez
} Horario_t;

// Inicializa a Serial2 (GP8/GP9) e cria o Mutex do relógio
void initRelogio();

// Protótipo da Task do FreeRTOS (chamar no xTaskCreate do main.cpp)
void vTaskRelogio(void *pvParameters);

// Função Thread-Safe para o Agendador e a Telemetria lerem a hora atual
Horario_t obterHorario();

#endif // RELOGIO_H 