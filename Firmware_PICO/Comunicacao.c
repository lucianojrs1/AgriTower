#include "Comunicacao.h"

void initComunicacao(){
    Serial1.begin(9600);
    filaDados = xQueueCreate(1, sizeof(DadosSensores));
}

void TaskComunicacao(void *pvParameters){

    DadosSensores dadosRecebidos;

    for(;;){
        if(xQueueReceive(filaDados, &dadosRecebidos, portMAX_DELAY) == pdPASS){
            Serial1.print("L:");
            Serial1.print(dadosRecebidos.luminosidade);
  
            Serial1.print(",T:");
            Serial1.print(dadosRecebidos.temperatura);

            Serial1.print(",B:");
            Serial1.println(dadosRecebidos.tensaoBateria);            
        }
    }
}