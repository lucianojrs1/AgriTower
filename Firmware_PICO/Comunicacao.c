#include "Comunicacao.h"

void initComunicacao(){
    Serial1.begin(115200);
    filaDados = xQueueCreate(1, sizeof(DadosSensores));
}

void TaskComunicacao(void *pvParameters){

    DadosSensores dadosRecebidos;

    for(;;){
        if(xQueueReceive(filaDados, &dadosRecebidos, portMAX_DELAY) == pdPASS){
            Serial1.print("L:");
            Serial1.print(dadosRecebidos.luminosidade);
  
            Serial1.print(",TAG:");
            Serial1.print(dadosRecebidos.tempAgua);

            Serial1.print(",TAR:");
            Serial1.print(dadosRecebidos.tempAr);

            Serial1.print(",U:");
            Serial1.println(dadosRecebidos.umidade);

            Serial1.print(",B:");
            Serial1.println(dadosRecebidos.tensaoBateria);            
        }
    }
}
