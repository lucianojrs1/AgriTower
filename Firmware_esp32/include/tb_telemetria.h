#pragma once

// Decide se é hora de enviar (periódico ou por mudança de bomba/modo),
// roda o modelo localmente e faz o POST ao ThingsBoard. Chamar a cada loop().
void verificarEEnviarTelemetria();
