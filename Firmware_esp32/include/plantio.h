#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// Dias desde o plantio (dia do plantio = dia 1).
// Retorna DIAS_SEM_RELOGIO se o NTP ainda não sincronizou.
int  calcularDiasCrescimento();

// Atualiza a data de plantio a partir do atributo do dashboard
// (aceita "AAAA-MM-DD", "DD/MM/AAAA" ou timestamp em ms). Retorna true se válida.
bool atualizarDataPlantio(JsonVariantConst valor);

// "DD/MM/AAAA" da data em uso
void formatarDataPlantio(char* destino, size_t tamanho);

// true se a data atual veio do dashboard; false se ainda é a do código
bool dataPlantioVeioDoDashboard();
