#ifndef BOMBA_H
#define BOMBA_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

void initBomba();

void TaskBomba(void *pvParameters);

#endif