#ifndef COMUNICACAO_H
#define COMUNICACAO_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <queue.h>

struct DadosSensores{
    int luminosidade;
    float temperatura;
    float tensaoBat;
};

extern QueueHandle_t filaDados;

void initComunicacao();
void TaskComunicacao(void *pvParameters);

#endif