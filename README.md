# AgriTower

<div align="center">
  <h1>🌱 AgriTower</h1>
  <h3>Controlador Inteligente de Irrigação e Torres Hidropônicas</h3>
  
  <p>
    <img src="https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++">
    <img src="https://img.shields.io/badge/Raspberry%20Pi%20Pico-A229B5?style=for-the-badge&logo=raspberrypi&logoColor=white" alt="Raspberry Pi Pico">
    <img src="https://img.shields.io/badge/FreeRTOS-0078C0?style=for-the-badge&logo=amazonaws&logoColor=white" alt="FreeRTOS">
    <img src="https://img.shields.io/badge/PlatformIO-FF7E00?style=for-the-badge&logo=platformio&logoColor=white" alt="PlatformIO">
    <img src="https://img.shields.io/badge/Arduino-00878F?style=for-the-badge&logo=arduino&logoColor=white" alt="Arduino">
  </p>
</div>

---

## 📖 Visão Geral

O **AgriTower** é um sistema embarcado de alta confiabilidade projetado para o controle preciso de irrigação e torres hidropônicas. O cérebro do sistema é uma **Raspberry Pi Pico**, operando sob o kernel de tempo real **FreeRTOS** para garantir multitarefa determinística e não-bloqueante. 

O sistema é responsável pela leitura de sensores, controle de atuadores e roteamento de dados via UART para um gateway IoT (ESP32), formando a base de uma solução agrícola moderna e conectada.

---

## 🏗️ Arquitetura de Software

O firmware foi desenvolvido utilizando uma arquitetura modular e orientada a objetos, separando claramente as responsabilidades de hardware e lógica. O uso do **FreeRTOS** garante que leituras de sensores, controle de atuadores e comunicação ocorram em paralelo sem interferências.

### ⚙️ Tasks do FreeRTOS
| Task | Período / Gatilho | Descrição |
| :--- | :--- | :--- |
| **`TaskSensores`** | 2000 ms (Tick) | Lê sensores analógicos/digitais, aplica matemática de condicionamento de sinal (divisores de tensão), empacota em uma `struct` e envia para a Fila (Queue). |
| **`TaskBomba`** | Contínua | Controla a bomba de 12V via PWM. Aplica *fade-in* (soft-start) para proteção mecânica, mantém ligada por 2s e descansa por 5s. Sincroniza com o LED de status. |
| **`TaskComunicacao`** | Bloqueante | Fica em estado *Blocked* aguardando a Queue do FreeRTOS. Ao receber a `struct`, formata e transmite via `Serial1` para o ESP32. |

---

## 🔌 Hardware e Pinout

O projeto conta com um circuito de condicionamento de sinal e proteção robusto, projetado para ambientes industriais úmidos e com variações de tensão.

| Componente | Pino RP2040 | Tipo | Detalhes do Circuito / Hardware |
| :--- | :---: | :---: | :--- |
| **Bomba 12V** | `GP4` | PWM Out | Acionada via transistor **TIP120**. Diodo Schottky **1N5822** em paralelo (flyback) para proteção contra corrente de retorno. |
| **Temp. (DS18B20)** | `GP22` | 1-Wire | Resistor pull-up de **4.7kΩ** ligado ao 3.3V. |
| **Luz (LDR)** | `GP26` | ADC0 | Divisor de tensão com resistor de **10kΩ** ligado ao 3.3V. |
| **Bateria (3S/12V)** | `GP27` | ADC1 | Divisor de tensão (**R1=47kΩ, R2=10kΩ**) para adequar os ~12.6V da bateria ao range de 3.3V do ADC. |
| **UART (TX)** | `GP0` | Serial | Conectado ao **RX** do ESP32 (Lógica 3.3V). |
| **UART (RX)** | `GP1` | Serial | Conectado ao **TX** do ESP32 (Lógica 3.3V). |

### 🔋 Sistema de Alimentação
* **Fonte Primária:** Bateria Li-ion 3S (~12.6V).
* **Regulação:** Módulo Step-Down **LM2596** ajustado para **5.0V**.
* **Entrada na Pico:** Alimentação injetada diretamente no pino **VSYS** (bypassando o regulador interno, garantindo maior eficiência).
* ⚠️ **Atenção:** Todos os GNDs (Bateria, LM2596, RP2040, ESP32 e Sensores) devem ser rigorosamente interligados.

---

## 📂 Estrutura do Projeto

O código segue o padrão do PlatformIO, separando cabeçalhos e implementações:

text
AgriTower/
│   ├── Bomba.h       
│   ├── Comunicacao.h 
│   └── Sensores.h     
│   ├── main.cp          # Setup do FreeRTOS e criação das Tasks
│   ├── Bomba.cp         # Setup de Inicialização da Task reponsável pela Bomba
│   ├── Comunicacao.cp   # Setup de Inicialização da Task reponsável pela Comunicação Serial 
│   └── Sensores.cp      # Setup de Inicialização da Task reponsável pelos Sensores 

---

## 📡 Protocolo de Comunicação (UART)

A Pico envia os dados para o ESP32 via `Serial1` (Baud rate padrão: `115200`). O payload é uma string formatada, terminada com quebra de linha (`\n`), facilitando o parsing no gateway:

**Formato:** `L:<luz>,T:<temp>,B:<bateria>\n`
**Exemplo:** `L:91,T:25.40,B:12.30\n`

* `L`: Porcentagem de luminosidade (0-100%)
* `T`: Temperatura em Celsius (°C)
* `B`: Tensão da bateria (V)

---

## 🗺️ Roadmap (Próximos Passos)
O sistema está em constante evolução. As próximas implementações incluem:

  **Nível de Água:** Adicionar sensor ultrassônico (à prova d'água) para proteção contra funcionamento a seco da bomba.
  
  **Qualidade da Água:** Integração de sensores de pH e Condutividade Elétrica (EC) para monitoramento de nutrientes.
  
  **Clima Externo:** Adicionar sensor para monitorar umidade e temperatura do ar no estufa.
  
  **Gateway IoT:** Desenvolver o firmware do ESP32 para receber os dados via UART e publicar via http (ThingsBoard).

---


