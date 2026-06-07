// radio_state.h
// Estado partilhado do radio + lista de presets.
#pragma once

#include <Arduino.h>
#include "config.h"

// Ecras disponiveis (corresponde aos 7 ecras do mockup).
enum Screen : uint8_t {
  SCREEN_MAIN = 0,   // 1. Ecra principal (radio em reproducao)
  SCREEN_TUNE,       // 2. Sintonia / ajuste de frequencia
  SCREEN_VOLUME,     // 3. Modo volume
  SCREEN_PRESETS,    // 4. Presets (favoritos)
  SCREEN_SCAN,       // 5. Scan (procura automatica)
  SCREEN_MENU,       // 6. Menu principal
  SCREEN_MESSAGE,    // 7. Mensagem / informacao
  SCREEN_SAVER       // protecao de ecra (anti burn-in) do ecra principal
};

struct Preset {
  char name[12];
  float freq;   // MHz
};

struct RadioState {
  float freq = 101.20f;     // frequencia atual (MHz)
  uint8_t volume = 14;      // 0..VOL_MAX
  uint8_t rssi = 0;         // 0..RSSI_BARS (barras preenchidas)
  bool stereo = true;       // STEREO vs MONO
  bool muted = false;       // mute ativo
  uint8_t step10 = 10;      // passo em centesimas de MHz (10 = 0.10)

  int8_t currentPreset = 2; // preset em reproducao (-1 = nenhum), 0-based
  uint8_t presetCursor = 0; // cursor no ecra de presets
  uint8_t menuCursor = 0;   // cursor no menu principal
  uint8_t pressedBtn = 0;   // botao fisico premido agora (0=nenhum, 1..4)

  bool autoScan = false;    // procura-e-grava automatica em curso
  uint8_t scanSlot = 0;     // memoria a preencher no scan automatico

  // Nome da estacao via RDS PS (Program Service, ate 8 chars). Vazio = mostra
  // a frequencia em vez do nome no ecra principal.
  char ps[12] = "Comercial";

  // Texto RDS RadioText recebido do Si4703 (ate 64 chars). Vazio = sem RDS.
  char rds[65] = "RADIO COMERCIAL * As melhores musicas sem paragens * 94.8 FM";

  // Texto para o ecra de mensagem (7).
  char msgTitle[20] = "ESTACAO GUARDADA";
  char msgLine[24]  = "RFM (89.50 MHz)";
  char msgFoot[20]  = "no PRESET 01";
};

// Lista de presets (favoritos). Valores iniciais iguais ao mockup.
// Os primeiros vêm preenchidos; os restantes ficam vazios (freq 0) ate serem
// gravados. O scan automatico/Guardar preenchem as memorias livres.
inline Preset presets[PRESET_COUNT] = {
  {"RFM",       93.20f},
  {"Comercial", 97.40f},
  {"Antena 1",  95.70f},
  {"M80",      104.30f},
  {"Mega Hits", 92.40f},
  {"Cidade FM", 91.60f},
};
