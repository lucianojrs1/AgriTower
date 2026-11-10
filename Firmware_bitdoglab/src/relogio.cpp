#include "relogio.h"

// --- Mapeamento de Pinos da UART1 (Serial2 na BitDogLab) ---
static const int pinTX_ESP = 8; // GP8 -> RX do ESP32
static const int pinRX_ESP = 9; // GP9 -> TX do ESP32

// --- Variáveis Privadas do Módulo (Encapsuladas com static) ---
static Horario_t horarioAtual = {12, 0, false}; // Inicia às 12:00 por padrão
static SemaphoreHandle_t xMutexRelogio = NULL;

// =========================================================================
// Inicialização da Serial2 e do Mutex (Chamar no setup() do main.cpp)
// =========================================================================
void initRelogio() {
    xMutexRelogio = xSemaphoreCreateMutex();

    // Configura UART1 nos pinos GP8 (TX) e GP9 (RX)
    Serial2.setTX(pinTX_ESP);
    Serial2.setRX(pinRX_ESP);
    Serial2.begin(9600);
    
    // IMPORTANTE PARA RTOS: Reduz o timeout de leitura de 1000ms para 50ms
    // Evita que readStringUntil bloqueie a Task por 1s caso venha lixo na serial
    Serial2.setTimeout(50);

    Serial.println("[INIT RELOGIO] Serial2 (GP8/GP9) iniciada a 9600 baud.");
}

// =========================================================================
// Função Thread-Safe para outras Tasks consultarem a hora
// =========================================================================
Horario_t obterHorario() {
    Horario_t copiaSegura = {12, 0, false};

    if (xMutexRelogio != NULL) {
        if (xSemaphoreTake(xMutexRelogio, pdMS_TO_TICKS(50)) == pdTRUE) {
            copiaSegura = horarioAtual;
            xSemaphoreGive(xMutexRelogio);
        }
    }
    return copiaSegura;
}

// =========================================================================
// TASK RTOS: Recepção de Hora via Serial2 e Fallback Interno de 60s
// =========================================================================
void vTaskRelogio(void *pvParameters) {
    (void) pvParameters;

    // Guarda o Tick do último avanço de minuto (equivale ao ultimoIncrementoMinuto)
    TickType_t xUltimoIncrementoMinuto = xTaskGetTickCount();
    const TickType_t xIntervaloUmMinuto = pdMS_TO_TICKS(60000); // 60.000 ms

    for (;;) {
        TickType_t xTempoAtual = xTaskGetTickCount();

        // -----------------------------------------------------------------
        // PARTE 1: Verifica se o ESP32 enviou horário novo ("T:14:35\n")
        // -----------------------------------------------------------------
        if (Serial2.available()) {
            String comando = Serial2.readStringUntil('\n');
            comando.trim();

            if (comando.length() > 0) {
                Serial.print("\n[SERIAL2 RX] Mensagem bruta recebida do ESP32: '");
                Serial.print(comando);
                Serial.println("'");

                if (comando.startsWith("T:")) {
                    int posDoisPontos = comando.indexOf(':', 2);
                    if (posDoisPontos != -1) {
                        int h = comando.substring(2, posDoisPontos).toInt();
                        int m = comando.substring(posDoisPontos + 1).toInt();

                        if (h >= 0 && h <= 23 && m >= 0 && m <= 59) {
                            // SEÇÃO CRÍTICA: Atualiza a hora protegida pelo Mutex
                            if (xMutexRelogio != NULL && xSemaphoreTake(xMutexRelogio, pdMS_TO_TICKS(100)) == pdTRUE) {
                                horarioAtual.hora = h;
                                horarioAtual.minuto = m;
                                horarioAtual.sincronizadoEsp = true;
                                xSemaphoreGive(xMutexRelogio);
                            }

                            // Reseta o contador de 60s local usando os Ticks atuais
                            xUltimoIncrementoMinuto = xTaskGetTickCount();

                            Serial.print("[RELOGIO] Horario sincronizado com sucesso -> ");
                            if (h < 10) Serial.print("0");
                            Serial.print(h);
                            Serial.print(":");
                            if (m < 10) Serial.print("0");
                            Serial.println(m);
                        } else {
                            Serial.println("[ERRO RELOGIO] Valores de hora/minuto fora do limite (00:00 - 23:59).");
                        }
                    }
                }
            }
        }

        // -----------------------------------------------------------------
        // PARTE 2: Fallback local (avança 1 min se passar 60s sem o ESP32)
        // -----------------------------------------------------------------
        if ((xTempoAtual - xUltimoIncrementoMinuto) >= xIntervaloUmMinuto) {
            xUltimoIncrementoMinuto += xIntervaloUmMinuto;

            int hPrint = 0, mPrint = 0;

            // SEÇÃO CRÍTICA: Incrementa o minuto protegendo com Mutex
            if (xMutexRelogio != NULL && xSemaphoreTake(xMutexRelogio, pdMS_TO_TICKS(100)) == pdTRUE) {
                horarioAtual.minuto++;
                if (horarioAtual.minuto >= 60) {
                    horarioAtual.minuto = 0;
                    horarioAtual.hora = (horarioAtual.hora + 1) % 24;
                }
                hPrint = horarioAtual.hora;
                mPrint = horarioAtual.minuto;
                xSemaphoreGive(xMutexRelogio);
            }

            Serial.print("\n[RELOGIO INTERNO] Avanco automatico de 1 minuto -> ");
            if (hPrint < 10) Serial.print("0");
            Serial.print(hPrint);
            Serial.print(":");
            if (mPrint < 10) Serial.print("0");
            Serial.println(mPrint);
        }

        // Dorme por 100ms (10 checagens por segundo na Serial2 é mais que suficiente
        // e deixa 99.9% do tempo de CPU livre para o resto do sistema)
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}