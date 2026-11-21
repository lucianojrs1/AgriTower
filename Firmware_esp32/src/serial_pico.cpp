#include "serial_pico.h"
#include "config.h"
#include "leds.h"

DadosTelemetriaPico ultimosDadosPico;
static String bufferSerial2 = "";

void iniciarSerialPico() {
  bufferSerial2.reserve(128);
  Serial2.setRxBufferSize(1024);
  Serial2.begin(BAUD_PICO, SERIAL_8N1, RXp2, TXp2);
}

void enviarLinhaPico(const char* linha) {
  Serial2.println(linha);
}

bool interpretarPacotePico(const String& linhaBruta) {
  // Procura "B:" mesmo que ruído de relé insira lixo no início da linha
  int posB = linhaBruta.indexOf("B:");
  if (posB == -1) return false;

  String pacoteLimpo = linhaBruta.substring(posB);
  pacoteLimpo.trim();

  DadosTelemetriaPico tmp;
  bool valido = false;

  // 1ª Tentativa: Formato completo (13 valores, E: e C: após A:)
  // Exemplo: B:12.2,L:80,TA:24.5,TR:26.0,U:65,P:6.20,S:650,A:1,E:1,C:3/15,T:14:35
  int lidos = sscanf(pacoteLimpo.c_str(),
                     "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,A:%d,E:%d,C:%d/%d,T:%d:%d",
                     &tmp.tensaoBat, &tmp.luminosidade, &tmp.tempAgua,
                     &tmp.tempAr, &tmp.umidade, &tmp.ph, &tmp.tds,
                     &tmp.modoAuto, &tmp.estadoBomba, &tmp.minCiclo, &tmp.tamCiclo,
                     &tmp.horaPico, &tmp.minPico);

  if (lidos == 13) {
    valido = true;
    tmp.temEstadoECiclo = true;
  } else {
    // 2ª Tentativa: Variação com E: e C: antes de A: (13 valores)
    lidos = sscanf(pacoteLimpo.c_str(),
                   "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,E:%d,C:%d/%d,A:%d,T:%d:%d",
                   &tmp.tensaoBat, &tmp.luminosidade, &tmp.tempAgua,
                   &tmp.tempAr, &tmp.umidade, &tmp.ph, &tmp.tds,
                   &tmp.estadoBomba, &tmp.minCiclo, &tmp.tamCiclo,
                   &tmp.modoAuto, &tmp.horaPico, &tmp.minPico);
    if (lidos == 13) {
      valido = true;
      tmp.temEstadoECiclo = true;
    } else {
      // 3ª Tentativa: Formato curto sem E: e C: (10 valores)
      lidos = sscanf(pacoteLimpo.c_str(),
                     "B:%f,L:%d,TA:%f,TR:%f,U:%d,P:%f,S:%d,A:%d,T:%d:%d",
                     &tmp.tensaoBat, &tmp.luminosidade, &tmp.tempAgua,
                     &tmp.tempAr, &tmp.umidade, &tmp.ph, &tmp.tds,
                     &tmp.modoAuto, &tmp.horaPico, &tmp.minPico);
      if (lidos == 10) {
        valido = true;
        tmp.temEstadoECiclo = false;
      }
    }
  }

  if (!valido) {
    Serial.printf("[SERIAL2] Pacote ignorado (%d campos lidos): [%s]\n", lidos, pacoteLimpo.c_str());
    return false;
  }

  tmp.disponivel  = true;
  tmp.pacoteBruto = pacoteLimpo;
  ultimosDadosPico = tmp;
  piscarLedSerial();
  return true;
}

void lerSerial2NaoBloqueante() {
  while (Serial2.available() > 0) {
    char c = (char)Serial2.read();
    if (c == '\r') continue;

    if (c == '\n') {
      if (bufferSerial2.length() > 0) {
        interpretarPacotePico(bufferSerial2);
        bufferSerial2 = "";
      }
    } else {
      bufferSerial2 += c;
      // Proteção contra estouro caso venha lixo sem quebra de linha
      if (bufferSerial2.length() > 256) {
        bufferSerial2 = "";
      }
    }
  }
}
