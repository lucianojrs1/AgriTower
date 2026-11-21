#include "plantio.h"
#include "time.h"
#include "config.h"

static int  plantioAno = PLANTIO_ANO;
static int  plantioMes = PLANTIO_MES;
static int  plantioDia = PLANTIO_DIA;
static bool plantioVeioDoDashboard = false;

int calcularDiasCrescimento() {
  if (DIAS_CRESCIMENTO_MANUAL >= 1) return DIAS_CRESCIMENTO_MANUAL;

  time_t agora = time(nullptr);
  if (agora < 1700000000) return DIAS_SEM_RELOGIO;

  struct tm hoje;
  localtime_r(&agora, &hoje);
  hoje.tm_hour = 12; hoje.tm_min = 0; hoje.tm_sec = 0;

  struct tm plantio = {};
  plantio.tm_year  = plantioAno - 1900;
  plantio.tm_mon   = plantioMes - 1;
  plantio.tm_mday  = plantioDia;
  plantio.tm_hour  = 12;
  plantio.tm_isdst = -1;

  double segundos = difftime(mktime(&hoje), mktime(&plantio));
  return (int)lround(segundos / 86400.0) + 1;
}

bool atualizarDataPlantio(JsonVariantConst valor) {
  int ano = 0, mes = 0, dia = 0;

  if (valor.is<const char*>()) {
    const char* texto = valor.as<const char*>();
    if (sscanf(texto, "%d-%d-%d", &ano, &mes, &dia) != 3 &&
        sscanf(texto, "%d/%d/%d", &dia, &mes, &ano) != 3) {
      ano = 0;
    }
  } else if (valor.is<double>()) {
    time_t segundos = (time_t)(valor.as<double>() / 1000.0);
    struct tm data;
    localtime_r(&segundos, &data);
    ano = data.tm_year + 1900;
    mes = data.tm_mon + 1;
    dia = data.tm_mday;
  }

  if (ano < 2020 || ano > 2100 || mes < 1 || mes > 12 || dia < 1 || dia > 31) {
    static unsigned long ultimoAviso = 0;
    if (ultimoAviso == 0 || millis() - ultimoAviso >= 60000) {
      ultimoAviso = millis();
      Serial.println("[DASHBOARD] data_plantio inválida (use AAAA-MM-DD, DD/MM/AAAA ou timestamp)");
    }
    return false;
  }

  if (!plantioVeioDoDashboard || ano != plantioAno || mes != plantioMes || dia != plantioDia) {
    Serial.printf("[DASHBOARD] Nova data de plantio sincronizada: %02d/%02d/%04d\n", dia, mes, ano);
  }
  plantioAno = ano;
  plantioMes = mes;
  plantioDia = dia;
  plantioVeioDoDashboard = true;
  return true;
}

void formatarDataPlantio(char* destino, size_t tamanho) {
  snprintf(destino, tamanho, "%02d/%02d/%04d", plantioDia, plantioMes, plantioAno);
}

bool dataPlantioVeioDoDashboard() {
  return plantioVeioDoDashboard;
}
