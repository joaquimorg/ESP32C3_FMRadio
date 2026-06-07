// main.cpp
// Radio FM (ESP32-C3 + SSD1306 + Si4703).
// Os 4 botoes fisicos navegam os ecras e controlam o radio.
#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "config.h"
#include "radio_state.h"
#include "ui.h"
#include "storage.h"
#include "radio_hw.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

RadioState state;
Screen screen = SCREEN_MAIN;
Screen returnScreen = SCREEN_MAIN;  // ecra para onde voltar apos uma mensagem
bool radioPresent = false;          // Si4703 detetado no arranque

// Estado anterior aplicado ao chip, para detetar alteracoes a aplicar.
float gPf = -1.0f;
int gPv = -1;
bool gPm = false;
bool gDirty = false;
unsigned long gChangeT = 0;

// Inatividade: no principal entra no protetor; noutros ecras volta ao principal.
constexpr unsigned long SAVER_TIMEOUT_MS = 30000;
unsigned long lastActivity = 0;

// --- Botoes -----------------------------------------------------------------
// Ativos em LOW (INPUT_PULLUP). Distinguem toque curto e toque longo.
const uint8_t BTN_PINS[4] = {BTN1_PIN, BTN2_PIN, BTN3_PIN, BTN4_PIN};
constexpr unsigned long DEBOUNCE_MS = 25;
constexpr unsigned long LONG_MS = 600;  // duracao a partir da qual e "longo"

bool btnDown[4] = {false, false, false, false};
bool btnLong[4] = {false, false, false, false};  // toque longo ja disparado
unsigned long btnStart[4] = {0, 0, 0, 0};
unsigned long btnChange[4] = {0, 0, 0, 0};

// --- Helpers de estado ------------------------------------------------------
void changeFreq(float delta) {
  state.freq += delta;
  if (state.freq < FM_MIN) state.freq = FM_MAX;
  if (state.freq > FM_MAX) state.freq = FM_MIN;
  state.currentPreset = -1;  // sintonia manual deixa de corresponder a preset
}

// Procura a proxima estacao (usa o seek do chip; senao avanca um passo).
// Marca a frequencia como ja aplicada para nao re-sintonizar no loop.
void doSeek() {
  if (RADIO_ENABLED) {
    radiohw::seekUp(state);
    gPf = state.freq;
    state.currentPreset = -1;
  } else {
    changeFreq(+FM_STEP);
  }
}

void showMessage(const char *title, const char *line, const char *foot) {
  strncpy(state.msgTitle, title, sizeof(state.msgTitle) - 1);
  strncpy(state.msgLine, line, sizeof(state.msgLine) - 1);
  strncpy(state.msgFoot, foot, sizeof(state.msgFoot) - 1);
  returnScreen = screen;
  screen = SCREEN_MESSAGE;
  lastActivity = millis();
}

// Verdadeiro se a string e vazia ou so tem espacos.
bool isBlank(const char *s) {
  for (; *s; s++)
    if (*s != ' ') return false;
  return true;
}

// Copia 'src' para 'dst' (cap) e remove espacos no fim.
void copyTrim(char *dst, size_t cap, const char *src) {
  strncpy(dst, src, cap - 1);
  dst[cap - 1] = '\0';
  for (int k = (int)strlen(dst) - 1; k >= 0 && dst[k] == ' '; k--) dst[k] = '\0';
}

// Grava dados numa memoria (preset) e persiste na NVS (sem UI).
void storePreset(uint8_t slot, float freq, const char *name) {
  if (slot >= PRESET_COUNT) return;
  presets[slot].freq = freq;
  if (name && !isBlank(name))  // usa o nome RDS, se existir
    copyTrim(presets[slot].name, sizeof(presets[slot].name), name);
  else
    presets[slot].name[0] = '\0';  // sem nome -> sera atualizado via RDS
  storage::savePresets();
}

// --- Ajudantes de memorias (ignoram as vazias) ------------------------------
int firstValidPreset() {
  for (uint8_t i = 0; i < PRESET_COUNT; i++)
    if (presets[i].freq > 0.0f) return i;
  return -1;
}
// Proxima/anterior memoria preenchida (dir = +1/-1), com wrap.
int adjacentValidPreset(int from, int dir) {
  for (int s = 1; s <= PRESET_COUNT; s++) {
    int i = ((from + dir * s) % PRESET_COUNT + PRESET_COUNT) % PRESET_COUNT;
    if (presets[i].freq > 0.0f) return i;
  }
  return from;
}
// Abre o ecra de presets, garantindo o cursor numa memoria preenchida.
void openPresets() {
  if (presets[state.presetCursor].freq <= 0.0f) {
    int v = firstValidPreset();
    if (v >= 0) state.presetCursor = (uint8_t)v;
  }
  screen = SCREEN_PRESETS;
}

// --- Scan automatico --------------------------------------------------------
// Usa o SEEK do chip (salta de estacao em estacao por hardware, sem parar no
// vazio) e, em cada estacao, espera por ESTEREO + nome RDS para gravar.
// Sobrepoe todas as memorias, a partir do inicio da banda.
constexpr unsigned long AS_STEREO_MS = 1500;  // espera por estereo na estacao
constexpr unsigned long AS_RDS_MS = 7000;     // espera pelo nome RDS (com estereo)
constexpr uint8_t AS_MAX_NUDGE = 2;           // passos finos apos o seek

enum AutoScanPhase { AS_SEEK, AS_WAIT };
AutoScanPhase asPhase = AS_SEEK;
unsigned long asT = 0;
char asLastName[12] = "";
bool asStereoSeen = false;  // ja se detetou estereo nesta estacao
uint8_t asNudge = 0;        // passos finos ja dados na estacao atual

void autoScanStop() { state.autoScan = false; }

void autoScanStart() {
  if (!RADIO_ENABLED) {
    showMessage("SCAN AUTO", "radio desativado", "");
    return;
  }
  // Sobrepoe TODAS as memorias.
  for (uint8_t i = 0; i < PRESET_COUNT; i++) {
    presets[i].freq = 0.0f;
    presets[i].name[0] = '\0';
  }
  asLastName[0] = '\0';
  state.scanSlot = 0;
  state.currentPreset = -1;
  state.autoScan = true;
  state.freq = FM_MIN;       // comeca no inicio da banda
  gPf = state.freq;          // o seek parte daqui
  if (RADIO_ENABLED) radiohw::tune(state);
  asPhase = AS_SEEK;
  asT = millis();
}

// Termina o scan com mensagem.
void autoScanFinish(const char *line) {
  autoScanStop();
  char foot[20];
  snprintf(foot, sizeof(foot), "%d memorias", state.scanSlot);
  showMessage("SCAN CONCLUIDO", line, foot);
}

void autoScanStep() {
  if (!state.autoScan) return;
  unsigned long now = millis();

  if (asPhase == AS_SEEK) {
    float before = state.freq;
    doSeek();                              // salta para a proxima estacao
    if (state.freq <= before) {            // deu a volta a banda -> fim
      autoScanFinish("fim da banda");
      return;
    }
    asStereoSeen = false;
    asNudge = 0;
    asPhase = AS_WAIT;
    asT = now;
    return;
  }

  // AS_WAIT: numa estacao; espera estereo + nome RDS para gravar.
  if (state.stereo) asStereoSeen = true;       // estereo e "pegajoso" (oscila)

  if (asStereoSeen && !isBlank(state.ps)) {    // estacao valida -> guarda
    if (strcmp(state.ps, asLastName) != 0) {   // evita duplicados
      storePreset(state.scanSlot, state.freq, state.ps);
      strncpy(asLastName, state.ps, sizeof(asLastName) - 1);
      asLastName[sizeof(asLastName) - 1] = '\0';
      state.currentPreset = state.scanSlot;
      Serial.printf("GRAVADO P%02d  %.2f  %s\n", state.scanSlot + 1, state.freq,
                    state.ps);
      state.scanSlot++;
      if (state.scanSlot >= PRESET_COUNT) {
        autoScanFinish("memorias cheias");
        return;
      }
    }
    asPhase = AS_SEEK;                          // procura a proxima estacao
  } else if (now - asT > AS_RDS_MS) {
    // Estereo presente mas sem nome RDS a tempo -> proxima estacao.
    Serial.printf("SEM RDS %.2f rssi=%u\n", state.freq, state.rssi);
    asPhase = AS_SEEK;
  } else if (!asStereoSeen && now - asT > AS_STEREO_MS) {
    // Sem estereo nem RDS: o seek pode ter parado antes do centro.
    if (asNudge < AS_MAX_NUDGE) {              // tenta 1-2 passos finos
      asNudge++;
      state.freq += FM_STEP;
      if (state.freq > FM_MAX) { asPhase = AS_SEEK; return; }
      Serial.printf("PASSO  %.2f (%d)\n", state.freq, asNudge);
      asT = now;                               // re-avalia na nova frequencia
    } else {
      Serial.printf("VAZIO  %.2f rssi=%u\n", state.freq, state.rssi);
      asPhase = AS_SEEK;                        // nada -> proxima estacao
    }
  }
}

// --- Maquina de estados da navegacao ---------------------------------------
void handleButton(uint8_t b) {
  switch (screen) {
    case SCREEN_MAIN:
      if (b == 1) changeFreq(-FM_STEP);
      else if (b == 2) changeFreq(+FM_STEP);
      else if (b == 3) screen = SCREEN_VOLUME;
      else if (b == 4) openPresets();
      break;

    case SCREEN_TUNE:
      if (b == 1) changeFreq(-state.step10 / 100.0f);
      else if (b == 2) changeFreq(+state.step10 / 100.0f);
      else if (b == 3) state.step10 = (state.step10 == 10) ? 5 : 10;  // 0.10/0.05
      else if (b == 4) screen = SCREEN_MAIN;  // sair (sem gravacao manual)
      break;

    case SCREEN_VOLUME:
      if (b == 1 && state.volume > VOL_MIN) state.volume--;
      else if (b == 2 && state.volume < VOL_MAX) state.volume++;
      else if (b == 3) state.muted = !state.muted;
      else if (b == 4) screen = SCREEN_MAIN;
      break;

    case SCREEN_PRESETS:
      if (b == 1) {  // anterior (salta vazias)
        state.presetCursor = (uint8_t)adjacentValidPreset(state.presetCursor, -1);
      } else if (b == 2) {  // seguinte (salta vazias)
        state.presetCursor = (uint8_t)adjacentValidPreset(state.presetCursor, +1);
      } else if (b == 3) {  // OK: sintonizar preset
        if (presets[state.presetCursor].freq > 0.0f) {
          state.freq = presets[state.presetCursor].freq;
          state.currentPreset = state.presetCursor;
          screen = SCREEN_MAIN;
        }
      } else if (b == 4) {
        screen = SCREEN_MAIN;
      }
      break;

    case SCREEN_SCAN:
      if (b == 1) {                       // STOP / sair
        if (state.autoScan) autoScanStop();
        else screen = SCREEN_MAIN;
      } else if (b == 2) {                // procurar proxima (manual)
        doSeek();
      } else if (b == 3) {                // AUTO: procurar e gravar / parar
        if (state.autoScan) autoScanStop(); else autoScanStart();
      } else if (b == 4) {                // sair para o menu
        autoScanStop();
        screen = SCREEN_MENU;
      }
      break;

    case SCREEN_MENU:
      if (b == 1) state.menuCursor = (state.menuCursor + 4) % 5;  // <
      else if (b == 2) state.menuCursor = (state.menuCursor + 1) % 5;  // >
      else if (b == 3) {  // OK
        switch (state.menuCursor) {
          case 0: screen = SCREEN_MAIN; break;
          case 1: openPresets(); break;
          case 2: screen = SCREEN_VOLUME; break;
          case 3: screen = SCREEN_SCAN; break;   // OPCOES -> Scan
          case 4: showMessage("FM RADIO v0.1", "ESP32-C3 + Si4703", "OLED 128x64"); break;
        }
      } else if (b == 4) screen = SCREEN_MAIN;  // SAIR
      break;

    case SCREEN_MESSAGE:
      // qualquer botao confirma e volta
      screen = returnScreen;
      break;

    case SCREEN_SAVER:
      // 1o toque so repoe o ecra completo; nao aciona nenhuma funcao.
      screen = SCREEN_MAIN;
      break;
  }
}

// Toque longo: atalhos a partir do ecra principal.
void handleLongPress(uint8_t b) {
  if (screen == SCREEN_MAIN) {
    if (b == 1) screen = SCREEN_MENU;  // longo no 1: abre o Menu
  }
}

// Le os 4 botoes, com debounce, e dispara toque curto / longo.
void processButtons() {
  unsigned long now = millis();
  for (int i = 0; i < 4; i++) {
    bool down = (digitalRead(BTN_PINS[i]) == LOW);
    if (down == btnDown[i]) {
      // Sem mudanca: verifica se um toque mantido ja chegou a "longo".
      if (down && !btnLong[i] && now - btnStart[i] >= LONG_MS) {
        btnLong[i] = true;
        lastActivity = now;
        handleLongPress(i + 1);
      }
      continue;
    }
    if (now - btnChange[i] < DEBOUNCE_MS) continue;  // ignora ressalto
    btnChange[i] = now;
    btnDown[i] = down;

    if (down) {                       // inicio do toque
      btnStart[i] = now;
      btnLong[i] = false;
      lastActivity = now;
      if (screen == SCREEN_SAVER) {   // 1o toque so sai do protetor de ecra
        screen = SCREEN_MAIN;
        btnLong[i] = true;            // consome (nao dispara curto na soltura)
      }
    } else {                          // soltura: se nao foi longo, e curto
      if (!btnLong[i]) handleButton(i + 1);
    }
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) pinMode(BTN_PINS[i], INPUT_PULLUP);

  // Memorias: abre a NVS e carrega presets + ultima frequencia/volume.
  storage::begin();
  storage::loadPresets();
  storage::loadSettings(state);

  // I2C inicializado UMA so vez: pelo radio (quando ativo) ou aqui. O Si4703
  // tem de ser o primeiro no barramento (faz o reset/seleccao 2-wire).
  if (RADIO_ENABLED) {
    radioPresent = radiohw::begin(state);  // faz o Wire.begin() do sistema
    Serial.println(radioPresent ? F("Si4703 OK") : F("Si4703 nao encontrado!"));
  } else {
    Wire.begin(SDA_PIN, SCL_PIN);
    radioPresent = false;
    Serial.println(F("Radio desativado (RADIO_ENABLED=false)."));
  }

  // OLED (Wire ja esta inicializado neste ponto).
  if (!ui::begin(display)) {
    Serial.println(F("SSD1306 nao encontrado!"));
    for (;;) delay(1000);
  }

  // Ecra de apresentacao animado durante 5 segundos.
  unsigned long t0 = millis();
  while (millis() - t0 < 5000) {
    ui::splash(display, (millis() - t0) / 5000.0f);
    delay(40);
  }

  randomSeed(esp_random());  // para a deriva do protetor de ecra
  lastActivity = millis();
  Serial.println(F("FM Radio pronto."));
}

void loop() {
  processButtons();  // toque curto/longo + saida do protetor de ecra

  // Feedback visual: realca a celula do botao que esta a ser premido.
  state.pressedBtn = 0;
  for (int i = 0; i < 4; i++)
    if (btnDown[i]) { state.pressedBtn = i + 1; break; }

  // Inatividade: principal -> protetor; outros ecras -> principal.
  // Excecao: durante o scan automatico, o ecra de scan permanece ativo.
  bool autoScanScreen = (screen == SCREEN_SCAN && state.autoScan);
  if (screen == SCREEN_MAIN && millis() - lastActivity > SAVER_TIMEOUT_MS) {
    screen = SCREEN_SAVER;
  } else if (screen != SCREEN_SAVER && screen != SCREEN_MAIN && !autoScanScreen &&
             millis() - lastActivity > SAVER_TIMEOUT_MS) {
    screen = SCREEN_MAIN;
  }

  // Aplica alteracoes ao Si4703 e persiste definicoes.
  if (state.freq != gPf) {
    if (RADIO_ENABLED) radiohw::tune(state);
    gPf = state.freq; gDirty = true; gChangeT = millis();
  }
  if (state.volume != gPv || state.muted != gPm) {
    if (RADIO_ENABLED) radiohw::applyVolume(state);
    gPv = state.volume; gPm = state.muted; gDirty = true; gChangeT = millis();
  }
  if (RADIO_ENABLED) radiohw::poll(state);  // RDS + RSSI/stereo
  autoScanStep();                           // scan automatico (se ativo)

  // Memoria sem nome a tocar: assim que o RDS dá o nome, atualiza e grava.
  if (!state.autoScan && state.currentPreset >= 0 && !isBlank(state.ps)) {
    Preset &p = presets[state.currentPreset];
    if (isBlank(p.name)) {
      copyTrim(p.name, sizeof(p.name), state.ps);
      storage::savePresets();
      Serial.printf("NOME P%02d = '%s'\n", state.currentPreset + 1, p.name);
    }
  }

  // Persiste a ultima frequencia/volume passado o periodo de inatividade.
  if (gDirty && millis() - gChangeT > 4000) {
    storage::saveSettings(state); gDirty = false;
  }

  ui::render(display, screen, state);
  delay(20);
}
