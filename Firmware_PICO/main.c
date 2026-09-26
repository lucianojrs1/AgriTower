#include <Arduino.h>
#include <FreeRTOS.h>
#include <task.h>
#include "Sensores.h"
#include "Bomba.h"


void setup() {
  Serial.begin(9600);
  analogReadResolution(12);

  initBomba();
  initSensores();

  xTaskCreate(TaskSensores, "Sensores", 1024, NULL, 1, NULL);
  xTaskCreate(TaskBomba, "Bomba", 1024, NULL, 1, NULL);

}

void loop() {
}