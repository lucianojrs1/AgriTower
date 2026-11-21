#pragma once

void iniciarSincronizacaoAtributos();    // Zera o temporizador (chamar no fim do setup())
bool sincronizarAtributosDashboard();    // GET do atributo compartilhado data_plantio
void sincronizarAtributosNoBoot();       // Primeira consulta no setup()
void gerenciarSincronizacaoAtributos();  // Consulta periódica / após reconexão; chamar no loop()
