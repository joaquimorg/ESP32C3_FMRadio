# ESP32-C3 FM Radio (Si4703 + OLED SSD1306)

Rádio FM controlado por um **ESP32-C3**, com módulo **Si4703**, ecrã **OLED SSD1306 128×64** e 4 botões físicos. Inclui RDS (nome da estação + RadioText), memórias (presets) persistentes, proteção de ecrã e ecrã de apresentação.

---

## Hardware

| Componente | Ligação | Notas |
|---|---|---|
| OLED SSD1306 | I2C `SDA=GPIO3`, `SCL=GPIO4`, addr `0x3C` | 128×64 |
| Si4703 | I2C partilhado (`SDA=3`, `SCL=4`) + `RST=GPIO20` | RST ativo em **HIGH** |
| Botão 1 | `GPIO5` | ativo em **LOW** (INPUT_PULLUP) |
| Botão 2 | `GPIO6` | ativo em **LOW** |
| Botão 3 | `GPIO7` | ativo em **LOW** |
| Botão 4 | `GPIO8` | ativo em **LOW** |

O barramento I2C é inicializado **uma única vez** (pelo rádio, quando ativo). Os botões ligam a GND e usam o pull-up interno.

### Esquema de ligações

![Esquema de ligações ESP32-C3, OLED SSD1306, Si4703 e botões](docs/wiring.svg)

### ⚠️ Importante: GPIO20 (RST) vs UART0

O RST do Si4703 usa o `GPIO20`, que por defeito é a **UART0 RX** do ESP32-C3. Para não haver conflito, o `Serial` é encaminhado para o **USB nativo** (USB Serial/JTAG) através de build flags em [`platformio.ini`](platformio.ini):

```ini
build_flags =
  -DARDUINO_USB_MODE=1
  -DARDUINO_USB_CDC_ON_BOOT=1
```

Assim a UART0 (GPIO20/21) fica livre para o RST e a consola série aparece no USB. **Sem isto, o RST não funciona e o RSSI lê sempre valores inválidos.**

### Ativar o rádio
Em [`include/config.h`](include/config.h):
```cpp
constexpr bool RADIO_ENABLED = true;   // false enquanto o Si4703 nao estiver ligado
```
Com `false`, o Si4703 não é inicializado (útil para testar só o interface/botões sem bloquear o I2C do OLED).

---

## Compilar e gravar (PlatformIO)

```bash
pio run                          # compilar
pio run -t upload --upload-port COM4   # gravar (ajustar a porta)
pio device monitor -b 115200     # consola serie
```

Bibliotecas (em [`platformio.ini`](platformio.ini)): Adafruit GFX, Adafruit SSD1306, Adafruit BusIO, mathertel/Radio. As memórias usam `Preferences` (NVS), já incluído no core do ESP32.

---

## Arranque

1. **Splash** (ecrã de apresentação) durante 5 segundos com barra de progresso.
2. Carrega as memórias e a última frequência/volume da NVS.
3. Entra no **ecrã Principal**.

---

## Ecrãs e botões

Os botões **1–4** são físicos; o ecrã mostra apenas a função de cada um na barra inferior. A célula do botão premido fica **invertida**.

### 1. Principal (rádio em reprodução)
Mostra o nome da estação (RDS) ou a frequência, RSSI, stereo/mono, preset atual e o RadioText (RDS) em scroll.

| Botão | Toque curto | Toque longo |
|---|---|---|
| 1 | Frequência − | **Abrir Menu** |
| 2 | Frequência + | — |
| 3 | Ir para Volume | — |
| 4 | Ir para Presets | — |

### 2. Sintonia (TUNE)
| Botão | Função |
|---|---|
| 1 | Frequência − (passo) |
| 2 | Frequência + (passo) |
| 3 | Alternar passo (0.10 / 0.05 MHz) |
| 4 | Sair (volta ao Principal) |

### 3. Volume
| Botão | Função |
|---|---|
| 1 | Volume − |
| 2 | Volume + |
| 3 | Mute on/off |
| 4 | OK (volta ao Principal) |

### 4. Presets (memórias)
Lista vertical com número, frequência e nome. A linha selecionada fica invertida. **Memórias vazias não são mostradas**; se não houver nenhuma, aparece `(sem memorias)`.

| Botão | Função |
|---|---|
| 1 | ↑ Anterior (salta vazias) |
| 2 | ↓ Seguinte (salta vazias) |
| 3 | OK (sintonizar o preset) |
| 4 | Sair |

### 5. Scan (procura automática)

| Botão | Função |
|---|---|
| 1 | Stop (parar auto-scan / voltar ao Principal) |
| 2 | Procurar a próxima estação (seek) |
| 3 | **AUTO** — procurar-e-gravar automático (alterna AUTO/PARA) |
| 4 | Sair para o Menu |

**Scan automático (AUTO):** começa no início da banda e **sobrepõe todas as memórias**. Usa o *seek* do chip (salta de estação em estação, sem parar no vazio) e, em cada estação, exige **estéreo + nome RDS** para gravar. Se o *seek* parar um pouco antes do centro (sem estéreo nem RDS), dá 1–2 passos finos de 0.1 MHz; se mesmo assim nada, segue para a próxima. Evita duplicados (pelo nome). Termina ao dar a volta à banda ou ao encher as memórias.

### 6. Menu principal
Ícones (o selecionado fica invertido):

| Ícone | Opção | Vai para |
| --- | --- | --- |
| 📻 | RADIO | Principal |
| ★ | PRESETS | Presets |
| 🔊 | VOLUME | Volume |
| 🔍 | SCAN | Scan |
| ℹ | SOBRE | Informação |

| Botão | Função |
| --- | --- |
| 1 | ◄ anterior |
| 2 | ► seguinte |
| 3 | OK (entrar) |
| 4 | Sair |

### 7. Mensagem / informação
Confirmações (ex.: "ESTAÇÃO GUARDADA"). Qualquer botão volta ao ecrã anterior.

### Proteção de ecrã (anti burn-in)
Ao fim de **30 s** parado no Principal, entra no protetor: nome + frequência a derivar lentamente e o RDS em scroll no fundo. O **primeiro toque** em qualquer botão apenas repõe o ecrã completo (não executa a função).

Nos outros ecrãs, ao fim de **30 s** sem botões, volta automaticamente ao **Principal**. A exceção é o ecrã de **Scan** enquanto o scan automático estiver em curso.

---

## Exemplos de ecrãs

![Exemplos dos ecrãs OLED 128x64](docs/screens-overview.svg)

As imagens individuais estão em [`docs/screens`](docs/screens). Para as regenerar depois de alterações visuais:

```bash
python tools/generate_docs_assets.py
```

---

## Memórias (presets)

- **20 memórias** (5 páginas de 4), guardadas na flash (NVS) — sobrevivem a reinícios.
- **Preenchidas pelo scan automático** (única forma de gravar) — só estações com estéreo + nome RDS. Não há gravação manual.
- **Ler/sintonizar**: ecrã de Presets, botão 3 (OK).
- **Atualização do nome via RDS**: se uma memória estiver **sem nome**, ao sintonizá-la e chegar o nome RDS, este é gravado automaticamente nessa memória.
- A última frequência e volume são também persistidos (após 4 s de inatividade).

---

## Estrutura do projeto

| Ficheiro | Descrição |
|---|---|
| [`src/main.cpp`](src/main.cpp) | Arranque, leitura de botões (curto/longo), máquina de estados |
| [`src/ui.h`](src/ui.h) / [`src/ui.cpp`](src/ui.cpp) | Desenho dos ecrãs e do splash |
| [`src/icons.h`](src/icons.h) | Ícones (bitmaps 16×16) |
| [`src/radio_state.h`](src/radio_state.h) | Estado do rádio, ecrãs e lista de presets |
| [`src/radio_hw.h`](src/radio_hw.h) / [`src/radio_hw.cpp`](src/radio_hw.cpp) | Controlo do Si4703 + RDS |
| [`src/storage.h`](src/storage.h) / [`src/storage.cpp`](src/storage.cpp) | Persistência (NVS) das memórias e definições |
| [`include/config.h`](include/config.h) | Pinos e parâmetros (FM, volume, `RADIO_ENABLED`) |

---

## Parâmetros (config.h)

| Parâmetro | Valor |
|---|---|
| Banda FM | 88.0 – 108.0 MHz |
| Passo de sintonia | 0.10 MHz |
| Volume | 0 – 30 |
| Barras RSSI | 0 – 5 (limiares `{8,16,26,36,44}` em [`radio_hw.cpp`](src/radio_hw.cpp)) |
| Memórias | 20 (5 páginas de 4) |
| Timeout do protetor de ecrã | 30 s |
| Timeout para voltar ao Principal | 30 s (exceto scan automático em curso) |
| Duração do toque longo | 600 ms |
| Espera de RDS no auto-scan | 6 s |

---
