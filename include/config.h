// config.h
// Definicoes globais de hardware e parametros do radio FM.
#pragma once

#include <Arduino.h>

// ---------------------------------------------------------------------------
// Display OLED SSD1306 128x64 (I2C)
// ---------------------------------------------------------------------------
constexpr uint8_t SDA_PIN = 3;
constexpr uint8_t SCL_PIN = 4;
constexpr uint8_t OLED_ADDR = 0x3C;

constexpr int SCREEN_WIDTH = 128;
constexpr int SCREEN_HEIGHT = 64;
constexpr int OLED_RESET = -1;

// ---------------------------------------------------------------------------
// Botoes fisicos (1, 2, 3, 4). Ativos em LOW (ligados a GND), INPUT_PULLUP.
// ---------------------------------------------------------------------------
constexpr uint8_t BTN1_PIN = 5;
constexpr uint8_t BTN2_PIN = 6;
constexpr uint8_t BTN3_PIN = 7;
constexpr uint8_t BTN4_PIN = 8;

// ---------------------------------------------------------------------------
// Modulo FM Si4703: I2C partilhado com o OLED (SDA=3, SCL=4) + RST (ativo HIGH)
// ---------------------------------------------------------------------------
constexpr uint8_t SI4703_RST_PIN = 20;

// Por a TRUE so quando o Si4703 estiver fisicamente ligado. Com FALSE, o
// radio nao e inicializado (evita bloquear o barramento I2C do OLED).
constexpr bool RADIO_ENABLED = true;

// ---------------------------------------------------------------------------
// Parametros da banda FM
// ---------------------------------------------------------------------------
constexpr float FM_MIN = 88.0f;    // MHz
constexpr float FM_MAX = 108.0f;   // MHz
constexpr float FM_STEP = 0.10f;   // passo de sintonia (MHz)

constexpr uint8_t VOL_MIN = 0;
constexpr uint8_t VOL_MAX = 30;

constexpr uint8_t RSSI_BARS = 5;    // numero de barras do indicador de sinal
constexpr uint8_t PRESET_COUNT = 20; // total de presets (5 paginas de 4)
