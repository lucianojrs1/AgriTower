#include "sensores.h"
#include <Wire.h>
#include <Adafruit_ADS1X15.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// --- Mapeamento de Pinos Exclusivos dos Sensores ---
static const int pinTempAgua = 18; // DS18B20 (IDC 14)
static const int pinDHT      = 19; // DHT22 (IDC 12)
static const int pinI2C_SDA  = 0;  // I2C0 SDA
static const int pinI2C_SCL  = 1;  // I2C0 SCL

// --- Constantes de Calibração ---
static const float R1_BAT = 47000.0f; 
static const float R2_BAT = 10000.0f;
static const float fatorDivisorPH = 2.0f;

// --- Instâncias Privadas (static = visíveis apenas neste arquivo) ---
static Adafruit_ADS1115 ads;
static OneWire oneWire(pinTempAgua);
static DallasTemperature sensorTempAgua(&oneWire);
static DHT dht(pinDHT, DHT22);

static bool adsConectado = false;

// --- Variáveis de Sincronização RTOS ---
static DadosSensores_t dadosAtuais = {0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};
static SemaphoreHandle_t xMutexSensores = NULL;

// =========================================================================
// Inicialização do Hardware e do Mutex (Chamar no setup() do main.cpp)
// =========================================================================
void initSensores() {
    // Cria o Mutex para proteger a leitura/escrita da struct
    xMutexSensores = xSemaphoreCreateMutex();

    // Configura sensores digitais
    sensorTempAgua.begin();
    // TRUQUE RTOS: Não trava a CPU esperando o DS18B20 converter!
    sensorTempAgua.setWaitForConversion(false); 
    
    dht.begin();
    Serial.println("[INIT SENSORES] DS18B20 e DHT22 configurados.");

    // Configura Barramento I2C0 e ADS1115
    Wire.setSDA(pinI2C_SDA);
    Wire.setSCL(pinI2C_SCL);
    Wire.begin();

    if (ads.begin(0x48)) {
        adsConectado = true;
        Serial.println("[INIT SENSORES] ADS1115 detectado no I2C0 (0x48)!");
    } else {
        adsConectado = false;
        Serial.println("[AVISO SENSORES] ADS1115 NAO encontrado! Leituras analogicas = 0.");
    }
}

// =========================================================================
// Função Thread-Safe para outras Tasks pegarem os dados mais recentes
// =========================================================================
DadosSensores_t obterDadosSensores() {
    DadosSensores_t copiaSegura = {0.0f, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, false};

    if (xMutexSensores != NULL) {
        // Tenta pegar a chave (Mutex) esperando até 50ms
        if (xSemaphoreTake(xMutexSensores, pdMS_TO_TICKS(50)) == pdTRUE) {
            copiaSegura = dadosAtuais; // Faz a cópia segura
            xSemaphoreGive(xMutexSensores); // Devolve a chave imediatamente
        }
    }
    return copiaSegura;
}

// =========================================================================
// TASK RTOS: Leitura Periódica dos Sensores (A cada 2000ms)
// =========================================================================
void vTaskSensores(void *pvParameters) {
    (void) pvParameters; // Evita warning de parâmetro não usado

    // Variáveis para garantir periodicidade exata de 2 segundos
    TickType_t xLastWakeTime = xTaskGetTickCount();
    const TickType_t xFrequencia = pdMS_TO_TICKS(2000);

    for (;;) {
        // Variáveis locais temporárias para fazer as leituras sem segurar o Mutex
        float tensaoBateria = 0.0f, valorTDS = 0.0f, valorPH = 0.0f;
        int luminosidade = 0;

        // 1. Dispara o pedido de conversão do DS18B20 no barramento 1-Wire
        sensorTempAgua.requestTemperatures();

        // 2. Enquanto o DS18B20 converte, já lemos o ADS1115 via I2C
        if (adsConectado) {
            int16_t brutoBat = ads.readADC_SingleEnded(0);
            tensaoBateria = (brutoBat * 0.0001875F) * ((R1_BAT + R2_BAT) / R2_BAT);

            int16_t brutoLDR = ads.readADC_SingleEnded(1);
            luminosidade = constrain(map(brutoLDR, 0, 17600, 0, 100), 0, 100);

            int16_t brutoTDS = ads.readADC_SingleEnded(2);
            float tensaoTDS = brutoTDS * 0.0001875F;
            valorTDS = (133.42f * pow(tensaoTDS, 3) - 255.86f * pow(tensaoTDS, 2) + 857.39f * tensaoTDS) * 0.5f;

            int16_t brutoPH = ads.readADC_SingleEnded(3);
            float tensaoRealPo = (brutoPH * 0.0001875F) * fatorDivisorPH;
            valorPH = 7.0f - ((2.5f - tensaoRealPo) / 0.18f);
        }

        // 3. Libera a CPU por 750ms para o DS18B20 terminar a conversão em background!
        // Outras tasks (PWM, Botão) rodam livres aqui.
        vTaskDelay(pdMS_TO_TICKS(750));

        // 4. Coleta a temperatura pronta do DS18B20
        float temp_agua = sensorTempAgua.getTempCByIndex(0);
        if (temp_agua == DEVICE_DISCONNECTED_C) {
            temp_agua = 0.0f;
        }

        // 5. Lê o DHT22 (Temperatura do Ar e Umidade)
        float temp_ar = dht.readTemperature();
        float umidade = dht.readHumidity();
        if (isnan(temp_ar)) temp_ar = 0.0f;
        if (isnan(umidade)) umidade = 0.0f;

        // 6. SEÇÃO CRÍTICA: Atualiza a struct global protegida pelo Mutex
        if (xMutexSensores != NULL) {
            if (xSemaphoreTake(xMutexSensores, pdMS_TO_TICKS(100)) == pdTRUE) {
                dadosAtuais.tensaoBateria = tensaoBateria;
                dadosAtuais.luminosidade  = luminosidade;
                dadosAtuais.valorTDS      = valorTDS;
                dadosAtuais.valorPH       = valorPH;
                dadosAtuais.tempAgua      = temp_agua;
                dadosAtuais.tempAr        = temp_ar;
                dadosAtuais.umidade       = umidade;
                dadosAtuais.adsOk         = adsConectado;
                
                xSemaphoreGive(xMutexSensores); // Libera o Mutex
            }
        }

        // 7. Dorme exatamente o tempo restante para fechar o ciclo de 2000ms
        vTaskDelayUntil(&xLastWakeTime, xFrequencia);
    }
}