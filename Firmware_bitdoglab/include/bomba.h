#ifndef BOMBA_H
#define BOMBA_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Máquina de Estados da Rampa PWM
enum EstadoBomba {
    DESLIGADA,
    LIGANDO,
    LIGADA,
    DESLIGANDO
};

// Estrutura para a Telemetria consultar o estado atual com segurança
typedef struct {
    EstadoBomba estado;
    int pwmAtual;
} StatusBomba_t;

// Inicializa os pinos da Bomba (GP4) e LED Verde (GP11) e cria o Mutex
void initBomba();

// Protótipo da Task do FreeRTOS (chamar no xTaskCreate do main.cpp)
void vTaskBomba(void *pvParameters);

// Função Thread-Safe para a Telemetria ler o PWM e o Estado da Bomba
StatusBomba_t obterStatusBomba();

// Função auxiliar para retornar o nome do estado da bomba no Monitor Série
const char* nomeEstadoBomba(EstadoBomba est);

#endif // BOMBA_H