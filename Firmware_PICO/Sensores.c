#include "Sensores.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include "Comunicacao.h"

const int pinBat = 27;
const int pinLDR = 26;
const int pinTemp = 22;

const float R1 = 47000.0; 
const float R2 = 10000.0;

OneWire oneWire(pinTemp);
DallasTemperature sensorTemp(&oneWire);

void initSensores() {

  pinMode(pinLDR, INPUT);
  pinMode(pinBat, INPUT);
  sensorTemp.begin(); 
}

void TaskSensores(void *pvParameters){

for(;;){
  Serial.println("--- Nova Leitura ---");

  // 1. Leitura do LDR
  int valorBrutoLDR = analogRead(pinLDR);
  int luminosidade = map(valorBrutoLDR, 0, 4095, 0, 100);
  
  // 2. Leitura da Temperatura da Água
  sensorTemp.requestTemperatures();                  
  float temperatura = sensorTemp.getTempCByIndex(0); 

  // 3. Tensão da Bateria
  int valorBrutoBat = analogRead(pinBat);
  float tensaoPino = (valorBrutoBat / 4095.0) * 3.3;
  float tensaoBateria = tensaoPino * ((R1 + R2) / R2);
  
  // 4. Exibição dos Dados no Monitor Série
  Serial.print("Luz Solar: ");
  Serial.print(luminosidade);
  Serial.print("% | Temp da Água: ");
  Serial.print(temperatura);
  Serial.println(" °C");
  Serial.print("Tensão da Bateria: ");
  Serial.print(tensaoBateria);
  Serial.println(" V");
  
  DadosSensores novosDados;
  novosDados.luminosidade = luminosidade;
  novosDados.temperatura = temperatura;
  novosDados.tensaoBateria = tensaoBateria;
  
  xQueueSend(filaDados, &novosDados, 0);
  vTaskDelay(pdMS_TO_TICKS(2000));
    }
}