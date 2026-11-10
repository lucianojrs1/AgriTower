#ifndef BOTAO_H
#define BOTAO_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

// Inicializa os pinos do Botão A (GP5) e do LED Vermelho (GP13) e cria o Mutex
void initBotao();

// Protótipo da Task do FreeRTOS (chamar no xTaskCreate do main.cpp)
void vTaskBotao(void *pvParameters);

// Retorna true se estiver em Modo Automático, ou false se estiver em Modo Manual
bool isModoAutomatico();

// Retorna true (apenas uma vez por clique) quando o usuário entra no Modo Manual,
// avisando a Task da Bomba que ela deve inverter seu estado (Ligar <-> Desligar)
bool consumirPedidoInversaoManual();

#endif // BOTAO_H