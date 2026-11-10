#include "oled.h"
#include "sensores.h"
#include "relogio.h"
#include "botao.h"
#include "agendamento.h"
#include "bomba.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

static const int screenWidth  = 128;
static const int screenHeight = 64;
static const int oledReset    = -1;

// Pinos nativos do OLED I2C1 na BitDogLab
static const int PINO_OLED_SDA = 14;
static const int PINO_OLED_SCL = 15;

// Passamos &Wire1 (I2C1) para separar do ADS1115 que usa &Wire (I2C0 nos pinos 0 e 1)
static Adafruit_SSD1306 display(screenWidth, screenHeight, &Wire1, oledReset);
static bool oledConectado = false;

void initOled() {
    // Configura o barramento I2C1 exclusivo do OLED da BitDogLab
    Wire1.setSDA(PINO_OLED_SDA);
    Wire1.setSCL(PINO_OLED_SCL);
    Wire1.begin();

    if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
        Serial.println("[ERRO OLED] Falha ao iniciar SSD1306 no endereco 0x3C (Wire1 GP14/GP15)!");
        oledConectado = false;
        return; // Nao trava o sistema todo com for(;;) caso o display falhe
    }

    oledConectado = true;
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 20);
    display.println("TORRE HIDROPONICA");
    display.setCursor(22, 36);
    display.println("Sistema Ativo");
    display.display();
}

void vTaskOled(void *pvParameters) {
    (void) pvParameters;

    // Aguarda 1s inicial para exibir a tela de boas-vindas sem usar delay() bloqueante
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (;;) {
        if (oledConectado) {
            DadosSensores_t     sens   = obterDadosSensores();
            Horario_t           relog  = obterHorario();
            StatusAgendamento_t agenda = obterStatusAgendamento();
            StatusBomba_t       bomba  = obterStatusBomba();
            bool                autoOn = isModoAutomatico();

            display.clearDisplay();
            display.setTextSize(1);
            display.setTextColor(SSD1306_WHITE);

            // --- LINHA 1 (Y=0): Relogio | Modo | Ciclo (Ex: "14:35 AUTO C:3/15") ---
            display.setCursor(0, 0);
            if (relog.hora < 10) display.print("0");
            display.print(relog.hora);
            display.print(":");
            if (relog.minuto < 10) display.print("0");
            display.print(relog.minuto);

            display.print(autoOn ? " AUTO " : " MAN  ");
            display.print("C:");
            display.print(agenda.minutoNoCiclo);
            display.print("/");
            display.print(agenda.tamanhoCiclo);

            // Linha divisoria horizontal abaixo do cabecalho
            display.drawLine(0, 9, 127, 9, SSD1306_WHITE);

            // --- LINHA 2 (Y=12): Bomba e PWM ---
            display.setCursor(0, 12);
            display.print("Bomba: ");
            if (bomba.pwm > 0) {
                int percentual = (bomba.pwm * 100) / 255;
                display.print("ON (");
                display.print(percentual);
                display.print("%)");
            } else {
                display.print("EM PAUSA");
            }

            // --- LINHA 3 (Y=23): Temp Agua e Luz ---
            display.setCursor(0, 23);
            display.print("Agua:");
            display.print(sens.tempAgua, 1);
            display.print("C Luz:");
            display.print(sens.luminosidade);
            display.print("%");

            // --- LINHA 4 (Y=34): Temp Ar e Umidade ---
            display.setCursor(0, 34);
            display.print("Ar:  ");
            display.print(sens.tempAr, 1);
            display.print("C Umid:");
            display.print(sens.umidadeAr, 0);
            display.print("%");

            // --- LINHA 5 (Y=45): pH e TDS ---
            display.setCursor(0, 45);
            display.print("pH:  ");
            display.print(sens.pH, 2);
            display.print("  TDS:");
            display.print(sens.tds, 0);

            // --- LINHA 6 (Y=56): Tensao de Alimentacao (Canal A0) ---
            display.setCursor(0, 56);
            display.print("Tensao: ");
            display.print(sens.tensaoBateria, 1);
            display.print("V");
            if (sens.tensaoBateria < 11.0 && sens.tensaoBateria > 5.0) {
                display.print(" [BAIXA!]");
            }

            display.display();
        }

        // Atualiza a tela a cada 500ms de forma nao-bloqueante no FreeRTOS
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}