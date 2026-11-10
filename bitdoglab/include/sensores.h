#ifndef SENSORES_H
#define SENSORES_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Estrutura que empacota todas as leituras dos sensores da torre
typedef struct {
    float tensaoBateria;
    int luminosidade;
    float valorTDS;
    float valorPH;
    float tempAgua;
    float tempAr;
    float umidade;
    bool adsOk; // Flag útil para diagnóstico
} DadosSensores_t;

// Inicializa o hardware dos sensores (I2C, ADS1115, OneWire, DHT) e cria o Mutex
void initSensores();

// Protótipo da Task do FreeRTOS (será chamada no xTaskCreate do main.cpp)
void vTaskSensores(void *pvParameters);

// Função Thread-Safe (protegida por Mutex) para outras Tasks lerem os sensores
DadosSensores_t obterDadosSensores();

#endif // SENSORES_H