#include "Bomba.h"

const int pinBomba = 4;
const int pinLed = LED_BUILTIN; 

void initBomba(){
    pinMode(pinBomba, OUTPUT);
}

void TaskBomba(void *pvParameters){

    for(;;){
        digitalWrite(pinLed, HIGH);

        for(int pot = 0; pot<=255; pot += 2){
            analogWrite(pinBomba, potencia);
            vTaskDelay(50);
        }

        vTaskDelay(2000);

        digitalWrite(pinLed, LOW);
        analogWrite(pinBomba, 0);

        vTaskDelay(2000);

    }

}
