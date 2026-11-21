#pragma once

// Conecta ao Wi-Fi (espera até timeoutWifiBoot). Retorna true se conectou.
bool iniciarWifi();

// Reconexão não-bloqueante; chamar a cada iteração do loop()
void gerenciarConexaoWifi();
