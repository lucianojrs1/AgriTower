#pragma once
#include <Arduino.h>

void iniciarNtp();                          // configTime() + zera temporizador
bool enviarHoraParaPico(uint32_t timeoutMs); // Envia "T:HH:MM" à BitDogLab
void sincronizarHoraNoBoot();               // Primeira tentativa (até 5 s) no setup()
void gerenciarEnvioHora();                  // Envio periódico; chamar a cada loop()
