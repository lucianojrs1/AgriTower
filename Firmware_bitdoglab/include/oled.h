#ifndef OLED_H
#define OLED_H

#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>

#define OLED_TASK_STACK_SIZE 2048
#define OLED_TASK_PRIORITY   1

void initOled();
void vTaskOled(void *pvParameters);
#endif