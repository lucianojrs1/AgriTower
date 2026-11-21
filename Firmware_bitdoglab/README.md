# AgriTower

<div align="center">
  <h1>🌱 AgriTower</h1>
  <h3>Controlador Inteligente de Irrigação e Torres Hidropônicas com Edge AI</h3>
  
  <p>
    <img src="https://img.shields.io/badge/C++-00599C?style=for-the-badge&logo=c%2B%2B&logoColor=white" alt="C++">
    <img src="https://img.shields.io/badge/Raspberry%20Pi%20Pico-A229B5?style=for-the-badge&logo=raspberrypi&logoColor=white" alt="Raspberry Pi Pico">
    <img src="https://img.shields.io/badge/ESP32-E7352C?style=for-the-badge&logo=espressif&logoColor=white" alt="ESP32">
    <img src="https://img.shields.io/badge/FreeRTOS-0078C0?style=for-the-badge&logo=amazonaws&logoColor=white" alt="FreeRTOS">
    <img src="https://img.shields.io/badge/PlatformIO-FF7E00?style=for-the-badge&logo=platformio&logoColor=white" alt="PlatformIO">
    <img src="https://img.shields.io/badge/ThingsBoard-305680?style=for-the-badge&logo=thingsboard&logoColor=white" alt="ThingsBoard">
  </p>
</div>

---

## 📖 Visão Geral

O **AgriTower** é um sistema embarcado de alta confiabilidade projetado para o monitoramento físico-químico, diagnóstico agronômico por Inteligência Artificial embarcada (*Edge AI*) e controle automatizado de irrigação em torres hidropônicas.

A arquitetura é dividida em dois subsistemas cooperativos:

1. **Unidade de Controle e Aquisição (BitDogLab V7 / Raspberry Pi Pico - RP2040):** Opera sob o kernel de tempo real **FreeRTOS** (Core *Earle Philhower*), garantindo execução multitarefa determinística, leitura de sensores analógicos de $16\text{ bits}$ e digitais, interface gráfica local em display OLED, agendamento fotoperiódico (Dia/Noite) e acionamento seguro de bomba submersível *brushless*.

2. **Gateway IoT e Inferência ML (ESP32):** Sincroniza o horário oficial via **NTP**, alimenta o relógio da BitDogLab via UART, executa localmente um modelo de **Árvore de Decisão (`DecisionTreeClassifier`)** aliado a análise estatística (*Z-score* e percentis) sobre os parâmetros da solução nutritiva e publica diagnósticos e telemetria via HTTPS no **ThingsBoard**.

---

## 🏗️ Arquitetura de Software

### 📊 Diagrama de Fluxo do Sistema

O diagrama abaixo ilustra o fluxo de execução concorrente das *Tasks* do FreeRTOS na BitDogLab, a troca de pacotes via `Serial2` e o pipeline de inferência da Árvore de Decisão e publicação HTTPS na ESP32:

<div align="center">
  <img src="docs/Fluxo.png" alt="Diagrama de Fluxo do Sistema AgriTower" width="850">
  <p><em>Figura 1: Diagrama de fluxo de software, escalonamento de Tasks RTOS e comunicação IoT.</em></p>
</div>

### 1. Firmware da BitDogLab (RP2040 + FreeRTOS)

O firmware do controlador principal foi projetado de forma modularizada, encapsulando variáveis de estado com proteção atômica via **Mutexes (`SemaphoreHandle_t`)** para comunicação *thread-safe* entre as tarefas.

#### ⚙️ Tasks do FreeRTOS

| Task | Período / Gatilho | Descrição | 
| :--- | :--- | :--- | 
| **`vTaskSensores`** | $2000\text{ ms}$ | Lê sensores digitais (`DS18B20` e `DHT22`) e o conversor A/D externo de $16\text{ bits}$ (`ADS1115` via `Wire` / I2C0) para tensão da fonte/bateria, luminosidade (`LDR`), `pH` e condutividade/sólidos dissolvidos (`TDS`), atualizando a `struct` protegida por Mutex. | 
| **`vTaskRelogio`** | $100\text{ ms}$ (Polling) | Escuta pacotes de sincronismo horário (`T:HH:MM`) enviados pelo ESP32 na `Serial2` e mantém um *fallback* interno de $60\text{ s}$ imune a *underflow* de `TickType_t` caso o gateway fique offline. | 
| **`vTaskAgendamento`** | $500\text{ ms}$ | Avalia o horário atual e define a janela de irrigação conforme o fotoperíodo: **Ciclo Diurno** ($06\text{h}00$ às $17\text{h}59$: $5\text{ min}$ ligada / $10\text{ min}$ em pausa) e **Ciclo Noturno** ($18\text{h}00$ às $05\text{h}59$: $5\text{ min}$ ligada / $45\text{ min}$ em pausa). | 
| **`vTaskBotoes`** | $20\text{ ms}$ (Debounce) | Monitora os botões físicos da placa para alternância imediata entre **Modo Automático** e **Modo Manual** (*override* direto da bomba). | 
| **`vTaskBomba`** | $50\text{ ms}$ | Máquina de estados que comanda a bomba *brushless* de $12\text{ V}$ em modo `ON/OFF` via módulo relé (`Active LOW`), aciona o LED Verde de status e emite alerta sonoro não-bloqueante no Buzzer passivo. | 
| **`vTaskOled`** | $500\text{ ms}$ | Renderiza no display OLED `SSD1306` ($128 \times 64$ no barramento dedicado `Wire1` / I2C1) o horário, modo de operação, progresso do ciclo, estado da bomba e todas as grandezas dos sensores. | 
| **`vTaskComunicacao`** | $500\text{ ms}$ | Coleta de forma *thread-safe* os estados de todos os módulos, transmite a string compacta via `Serial2` para o ESP32, imprime o painel de diagnóstico na USB Serial e pulsa o LED Azul de telemetria. | 

### 2. Gateway IoT e Edge AI (ESP32)

O firmware da ESP32 atua como coprocessador de conectividade e inteligência artificial:

* **Árvore de Decisão Embarcada (`Eloquent::ML`):** Modelo treinado a partir do *Lettuce Dataset* ($3.169$ registros), recebendo o vetor de atributos $x = [\text{Temperatura do Ar}, \text{Umidade}, \text{TDS}, \text{pH}, \text{Dias de Crescimento}]$ para classificar a planta em:
  * `0`: **Crescendo**
  * `1`: **Pronto para colher**
  * `2`: **Estragado** (com alerta imediato de inspeção da torre)
* **Diagnóstico Estatístico Contextual:** Compara cada leitura com a média ($\mu$) e o desvio padrão ($\sigma$) esperados para a idade exata do cultivo (dias $1$ a $48$), gerando recomendações agronômicas automáticas quando $\vert Z\vert > 2$ ou fora dos percentis $P_5\text{--}P_{95}$.
* **Sincronização Dinâmica com o Dashboard:** Consulta atributos compartilhados no ThingsBoard (`data_plantio`) para recalcular automaticamente a idade da cultura e mantém o RP2040 sincronizado via NTP ($\text{GMT}-3$).
* **Execução Cooperativa (`loop`):** O firmware da ESP32 (framework Arduino) executa as rotinas em um laço cooperativo, com as seguintes periodicidades:

  | Rotina | Periodicidade |
  | :--- | :---: |
  | Publicação de telemetria via HTTPS no ThingsBoard | $2\text{ s}$ |
  | Leitura do atributo compartilhado `data_plantio` | $5\text{ s}$ |
  | Envio da hora NTP para a BitDogLab (`Serial2`) | $60\text{ s}$ |

---

## 🔌 Hardware e Pinout

### ⚡ Diagrama Eletrônico (Esquemático)

Abaixo encontra-se o esquemático completo de interligação entre a **BitDogLab (RP2040)**, o **Gateway ESP32**, o conversor **ADS1115**, os condicionadores de sensores (`pH`, `TDS`, `LDR`, `DS18B20`, `DHT22`) e o estágio de potência com **Módulo Relé** para a bomba *brushless* de $12\text{ V}$:

<div align="center">
  <img src="docs/Esquematico.png" alt="Diagrama Eletrônico do Sistema AgriTower" width="850">
  <p><em>Figura 2: Esquemático elétrico da PCB, condicionamento de sinais e acionamento de potência.</em></p>
</div>

### Mapeamento da BitDogLab (RP2040) e Periféricos Externos

| Componente / Função | Pino RP2040 / Canal | Barramento / Tipo | Detalhes do Circuito / Hardware | 
| :--- | :---: | :---: | :--- | 
| **Bomba Brushless 12V** | `GP4` | Digital Out | Bomba submersível ($4.8\text{ W}$, $240\text{ L/h}$) chaveada via Módulo Relé `SRD-05VDC-SL-C` (*Active LOW*, $V_{CC} = 3.3\text{ V}$) com diodo *flyback* e capacitor de $100\text{ nF}$ nos bornes. | 
| **Botão A (Override)** | `GP5` | Digital In | `INPUT_PULLUP` interno — inverte manualmente o estado da bomba no Modo Manual. | 
| **Botão B (Modo)** | `GP6` | Digital In | `INPUT_PULLUP` interno — alterna entre **Modo Automático** e **Modo Manual**. | 
| **UART1 TX (Pico** $\rightarrow$ **ESP32)** | `GP8` | `Serial2` TX | Conectado ao pino `RX2` (`GPIO16`) do ESP32 ($9600\text{ baud}$, lógica $3.3\text{ V}$). | 
| **UART1 RX (ESP32** $\rightarrow$ **Pico)** | `GP9` | `Serial2` RX | Conectado ao pino `TX2` (`GPIO17`) do ESP32 ($9600\text{ baud}$, lógica $3.3\text{ V}$). | 
| **Buzzer B (Alerta)** | `GP10` | PWM (Slice 5) | Buzzer eletromagnético passivo — emite bipe de $2.5\text{ kHz}$ ($150\text{ ms}$) na partida da bomba. | 
| **LED Verde (Bomba)** | `GP11` | Digital Out | Indica bomba energizada / relé atracado (`ON`). | 
| **LED Azul (Telemetria)** | `GP12` | Digital Out | Pulsa por $50\text{ ms}$ a cada pacote transmitido para o ESP32. | 
| **Display OLED SSD1306** | `GP14` / `GP15` | `Wire1` (I2C1) | `SDA1` no `GP14`, `SCL1` no `GP15` a $400\text{ kHz}$ (endereço `0x3C`), isolado do barramento dos sensores. | 
| **Conversor ADC ADS1115** | `GP0` / `GP1` | `Wire` (I2C0) | ADC de $16\text{ bits}$ para leitura imune a ruído de `A0` (Tensão $12\text{ V}$), `A1` (`LDR`), `A2` (`pH`) e `A3` (`TDS`). | 
| **Temp. Água (DS18B20)** | `GP18` | 1-Wire | Sonda impermeável com resistor *pull-up* de $4.7\text{ k}\Omega$ ($2.2\text{ k}\Omega$ para cabos longos) ao $3.3\text{ V}$. | 
| **Clima (DHT22)** | `GP19` | 1-Wire Prop. | Sensor de temperatura ($^\circ\text{C}$) e umidade relativa do ar ($\%$). | 

### 🔋 Alimentação e Cabeamento Externo

* **Fonte Primária:** Fonte Chaveada Colmeia de $12\text{ V}$ / $5\text{ A}$ ajustada em $12.2\text{ V}$ para a linha de potência da bomba submersível.
* **Regulação Lógica:** Conversor Step-Down DC-DC entregando alimentação estabilizada para a placa e módulos sensores.
* **Cabeamento para Ambiente Externo:** Uso de **Cabo Manga Blindado** (ou CAT5e F/UTP de dupla capa com proteção UV) para os sinais dos sensores (com malha aterrada exclusivamente no `GND` da PCB) e **Cabo Paralelo** $2 \times 1.5\text{ mm}^2$ separado fisicamente para a alimentação da bomba $12\text{ V}$.

---

## 📂 Estrutura do Projeto

O repositório está organizado no padrão do **PlatformIO**, separando os módulos de domínio da BitDogLab (cabeçalhos em `include/` e implementações em `src/`), o firmware do gateway ESP32, o `dataset` e a documentação visual na pasta `docs/`:

```
AgriTower/
├── .vscode/                    # Configurações do editor (VS Code)
├── dataset/                    # Dataset do projeto
├── docs/
│   ├── Fluxo.png               # Fluxograma das Tasks RTOS e pipeline IoT/ML
│   └── Esquematico.png         # Esquemático elétrico completo da placa e sensores
│
├── Firmware_bitdoglab/
│   ├── platformio.ini          # Configuração do RP2040 (Core Earle Philhower + FreeRTOS)
│   ├── include/                # Cabeçalhos (.h) dos módulos
│   │   ├── agendamento.h
│   │   ├── bomba.h
│   │   ├── botao.h
│   │   ├── comunicacao.h
│   │   ├── oled.h
│   │   ├── relogio.h
│   │   └── sensores.h
│   └── src/
│       ├── main.cpp            # Inicialização de periféricos e criação das Tasks do FreeRTOS
│       ├── agendamento.cpp     # Lógica de ciclos Diurno (5m/10m) e Noturno (5m/45m)
│       ├── bomba.cpp           # Máquina de estados do Relé (GP4), LED Verde (GP11) e Buzzer (GP10)
│       ├── botao.cpp           # Leitura com debounce dos botões de Modo (Auto/Manual) e Override
│       ├── comunicacao.cpp     # Transmissão Serial2 para o ESP32 e painel de debug USB
│       ├── oled.cpp            # Interface gráfica local no display SSD1306 via Wire1 (GP14/GP15)
│       ├── relogio.cpp         # Recepção de hora NTP via Serial2 e fallback interno de 60s
│       └── sensores.cpp        # Aquisição do ADS1115 (Bateria, LDR, pH, TDS), DS18B20 (GP18) e DHT22 (GP19)
│
└── Firmware_esp32/
    ├── platformio.ini          # Configuração da ESP32 e biblioteca ArduinoJson
    └── src/
        └── main.cpp            # Gateway Wi-Fi/HTTPS ThingsBoard, cliente NTP e Árvore de Decisão ML
```

---

## 📡 Protocolo de Comunicação Bidirecional (`Serial2` @ $9600\text{ baud}$)

### 1. Telemetria: BitDogLab $\rightarrow$ ESP32 (A cada $500\text{ ms}$)

A BitDogLab envia uma string estruturada em chave-valor finalizada por quebra de linha (`\n`):

**Formato Completo (11 campos):**

```
B:<tensao>,L:<luz>,TA:<temp_agua>,TR:<temp_ar>,U:<umid>,P:<ph>,S:<tds>,A:<auto>,E:<bomba>,C:<min>/<ciclo>,T:<HH>:<MM>\n
```

**Exemplo Real:**

```
B:12.2,L:84,TA:24.8,TR:27.3,U:65,P:6.35,S:590,A:1,E:1,C:3/15,T:14:35\n
```

* `B`: Tensão de alimentação/bateria ($\text{V}$)
* `L`: Luminosidade ambiente ($0\text{--}100\%$)
* `TA`: Temperatura da solução nutritiva no reservatório ($^\circ\text{C}$)
* `TR`: Temperatura do ar externo ($^\circ\text{C}$)
* `U`: Umidade relativa do ar ($\%$)
* `P`: Potencial hidrogeniônico da solução ($\text{pH}$)
* `S`: Sólidos Totais Dissolvidos ($\text{TDS}$ em $\text{ppm}$)
* `A`: Modo de operação (`1` = Automático, `0` = Manual)
* `E`: Estado físico do relé da bomba (`1` = Ligada, `0` = Em pausa)
* `C`: Minuto atual / Tamanho total do ciclo ativo (`3/15` de dia ou `12/50` à noite)
* `T`: Horário atual sincronizado no controlador (`HH:MM`)

### 2. Sincronismo Horário: ESP32 $\rightarrow$ BitDogLab (A cada $60\text{ s}$)

Assim que o ESP32 obtém a hora oficial via servidor NTP (`pool.ntp.org`), ele atualiza o relógio interno da BitDogLab enviando:

```
T:14:35\n
```

---

## 🗺️ Roadmap e Evolução do Projeto

* [x] **Arquitetura Multitarefa:** Migração completa para **FreeRTOS** com sincronização por Mutexes.
* [x] **Qualidade da Água e Clima:** Integração dos sensores de `pH`, `TDS` (via ADC de $16\text{ bits}$ `ADS1115`), `DS18B20` e `DHT22`.
* [x] **Interface Local e Alerta Sonoro:** Exibição em tempo real no display OLED (`Wire1`) e aviso de partida no Buzzer (`GP10`).
* [x] **Estágio de Potência para Bomba Brushless:** Migração de PWM para chaveamento direto `ON/OFF` via módulo relé isolado.
* [x] **Gateway IoT e Edge AI:** Firmware da ESP32 com publicação HTTPS no **ThingsBoard**, sincronismo NTP e **Árvore de Decisão** embarcada para diagnóstico da alface.
* [ ] **Sensor de Nível do Reservatório:** Integrar sensor ultrassônico à prova d'água ou boia de nível para bloqueio de segurança contra funcionamento a seco.
* [ ] **Dosagem Automatizada (Peristálticas):** Controle de bombas peristálticas para correção automática de `pH` e condutividade (`TDS`).