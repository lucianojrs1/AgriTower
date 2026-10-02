#ifndef SENSORES_H
#define SENSORES_H
#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>



// Função para configurar os pinos dos sensores
void initSensores();

// A Task do FreeRTOS que fará a leitura contínua
void TaskSensores(void *pvParameters);

#endif