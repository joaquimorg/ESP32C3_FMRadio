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
constexpr uint8_t BTN1_PIN = 8;
constexpr uint8_t BTN2_PIN = 7;
constexpr uint8_t BTN3_PIN = 6;
constexpr uint8_t BTN4_PIN = 5;

// ---------------------------------------------------------------------------
// Modulo FM Si4703: I2C partilhado com o OLED (SDA=3, SCL=4) + RST (ativo HIGH)
// ---------------------------------------------------------------------------
constexpr uint8_t SI4703_RST_PIN = 20;

// Por a TRUE so quando o Si4703 estiver fisicamente ligado. Com FALSE, o
// radio nao e inicializado (evita bloquear o barramento I2C do OLED).
constexpr bool RADIO_ENABLED = true;

// ---------------------------------------------------------------------------
// Integracao de audio com o IKEA KALLSUP.
// GPIO0/1 sao GPIOs gerais livres na ESP32-C3-DevKitM-1 e nao sao pinos de
// strapping. Inverter os niveis abaixo se a logica externa assim o exigir.
// Usar resistores externos para garantir BT + AMP OFF enquanto o ESP32 reinicia.
// ---------------------------------------------------------------------------
constexpr uint8_t PIN_AUDIO_SELECT = 0;
constexpr uint8_t PIN_AMP_CTRL = 1;

constexpr uint8_t AUDIO_SELECT_BT_LEVEL = LOW;
constexpr uint8_t AUDIO_SELECT_FM_LEVEL = HIGH;
constexpr uint8_t AMP_CTRL_OFF_LEVEL = LOW;
constexpr uint8_t AMP_CTRL_ON_LEVEL = HIGH;

static_assert(PIN_AUDIO_SELECT != PIN_AMP_CTRL,
              "AUDIO_SELECT e AMP_CTRL nao podem partilhar GPIO");
static_assert(PIN_AUDIO_SELECT != SDA_PIN && PIN_AUDIO_SELECT != SCL_PIN &&
              PIN_AUDIO_SELECT != BTN1_PIN && PIN_AUDIO_SELECT != BTN2_PIN &&
              PIN_AUDIO_SELECT != BTN3_PIN && PIN_AUDIO_SELECT != BTN4_PIN &&
              PIN_AUDIO_SELECT != SI4703_RST_PIN,
              "PIN_AUDIO_SELECT colide com hardware existente");
static_assert(PIN_AMP_CTRL != SDA_PIN && PIN_AMP_CTRL != SCL_PIN &&
              PIN_AMP_CTRL != BTN1_PIN && PIN_AMP_CTRL != BTN2_PIN &&
              PIN_AMP_CTRL != BTN3_PIN && PIN_AMP_CTRL != BTN4_PIN &&
              PIN_AMP_CTRL != SI4703_RST_PIN,
              "PIN_AMP_CTRL colide com hardware existente");
static_assert(AUDIO_SELECT_BT_LEVEL != AUDIO_SELECT_FM_LEVEL,
              "Os niveis BT e FM devem ser diferentes");
static_assert(AMP_CTRL_OFF_LEVEL != AMP_CTRL_ON_LEVEL,
              "Os niveis OFF e ON do amplificador devem ser diferentes");

// Tempos de guarda para comutacao sem pops/clicks.
constexpr unsigned long FM_MUTE_SETTLE_MS = 20;
constexpr unsigned long FM_AMP_SETTLE_MS = 30;
constexpr unsigned long AUDIO_SWITCH_SETTLE_MS = 15;

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
