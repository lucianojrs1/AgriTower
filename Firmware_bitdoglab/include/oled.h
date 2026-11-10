#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

void initOled();
void vTaskOled(void *pvParameters);
#endif