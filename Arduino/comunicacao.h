#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

// Inicializa o pino do LED Azul de telemetria (GP12)
void initComunicacao();

// Protótipo da Task do FreeRTOS (chamar no xTaskCreate do main.cpp)
void vTaskComunicacao(void *pvParameters);

#endif // COMUNICACAO_H