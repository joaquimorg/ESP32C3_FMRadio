// radio_hw.cpp
// Controlo do Si4703 + descodificacao RDS (PS e RadioText) para o estado.
#include "radio_hw.h"
#include "config.h"
#include <Wire.h>
#include <math.h>
#include <SI4703.h>
#include "RDSParserLocal.h"

static SI4703 radio;
static RDSParserLocal rdsParser;
static RadioState *g_st = nullptr;

// Frequencia em unidades de 10 kHz usadas pela biblioteca (101.20 MHz -> 10120).
static uint16_t mhzToUnits(float mhz) { return (uint16_t)lroundf(mhz * 100.0f); }

// Callback do parser: nome curto da estacao (PS).
static void onServiceName(const char *name) {
  if (!g_st) return;
  strncpy(g_st->ps, name, sizeof(g_st->ps) - 1);
  g_st->ps[sizeof(g_st->ps) - 1] = '\0';
  Serial.printf("PS: '%s'\n", g_st->ps);  // debug: nome RDS recebido
}

// Callback do parser: RadioText (texto longo), entregue de forma progressiva
// (o prefixo cresce enquanto chegam mais segmentos - ver RDSParserLocal.cpp).
static void onRadioText(const char *txt) {
  if (!g_st) return;
  char clean[65];
  size_t n = 0;
  for (const char *p = txt; *p && n < sizeof(clean) - 1; p++)
    clean[n++] = (*p >= 0x20 && *p < 0x7F) ? *p : ' ';
  clean[n] = '\0';
  if (n == 0) return;  // ignora vazio (reset/erros) para nao apagar texto valido
  strncpy(g_st->rds, clean, sizeof(g_st->rds) - 1);
  g_st->rds[sizeof(g_st->rds) - 1] = '\0';
  Serial.printf("RDS-TXT [%2u]: '%s'\n", (unsigned)n, clean);  // debug
}

// Leitura direta do RDS no SI4703 (registos 0x0A..0x0F), em vez de radio.checkRDS().
//
// Porque: o checkRDS() da biblioteca so entrega o grupo se TODOS os 4 blocos
// forem corrigiveis. No RadioText os caracteres estao nos blocos C e D e o
// indice no bloco B; o bloco A (codigo PI) NAO e preciso. Bastava um erro
// irrecuperavel no bloco A para descartar o grupo inteiro e perder texto bom,
// fazendo a mensagem nunca completar. Aqui ignoramos erros do bloco A.
//
// Bonus: RSSI/estereo saem do mesmo registo 0x0A, evitando uma leitura extra
// (getRadioInfo) que limpava o flag RDSR e tambem fazia perder grupos.
static const uint8_t SI4703_I2C_ADDR = 0x10;

static void pollSI4703RDS(RadioState &st) {
  // le 0x0A..0x0F (status + RDS) numa so transacao (6 registos = 12 bytes)
  if (Wire.requestFrom((int)SI4703_I2C_ADDR, 12) != 12) return;
  uint16_t reg[6];
  for (int i = 0; i < 6; i++) {
    uint8_t hi = Wire.read();
    uint8_t lo = Wire.read();
    reg[i] = ((uint16_t)hi << 8) | lo;
  }
  uint16_t statusRssi = reg[0];  // 0x0A
  uint16_t readChan   = reg[1];  // 0x0B

  st.stereo = (statusRssi & 0x0100) != 0;

  // RSSI (0..~75) -> 0..5 barras por limiares (calibrado p/ Si4703).
  const uint8_t th[RSSI_BARS] = {8, 16, 26, 36, 44};
  uint8_t bars = 0;
  for (uint8_t i = 0; i < RSSI_BARS; i++)
    if ((statusRssi & 0x00FF) >= th[i]) bars++;
  st.rssi = bars;

  if (!(statusRssi & 0x8000)) return;  // RDSR: sem grupo novo pronto

  // indicadores de erro por bloco (0=ok, 1/2=corrigido, 3=irrecuperavel)
  uint8_t blerA = (statusRssi >> 9) & 3;
  uint8_t blerB = (readChan   >> 14) & 3;
  uint8_t blerC = (readChan   >> 12) & 3;
  uint8_t blerD = (readChan   >> 10) & 3;

  // Para o texto precisamos de B, C e D fiaveis; o bloco A nao importa.
  if (blerB == 3 || blerC == 3 || blerD == 3) {
    Serial.printf("RDS drop  bler A%u B%u C%u D%u  raw %04X %04X %04X %04X\n",
                  blerA, blerB, blerC, blerD, reg[2], reg[3], reg[4], reg[5]);  // debug
    return;
  }

  // Se o bloco A vier irrecuperavel, passamos 0xFFFF (nao-zero) para o parser
  // nao o confundir com o sinal de reset (block1 == 0).
  uint16_t blockA = (blerA == 3) ? 0xFFFF : reg[2];
  Serial.printf("RDS group %04X %04X %04X %04X  bler A%u B%u C%u D%u\n",
                blockA, reg[3], reg[4], reg[5], blerA, blerB, blerC, blerD);  // debug
  rdsParser.processData(blockA, reg[3], reg[4], reg[5]);  // RDSA, RDSB, RDSC, RDSD
}

namespace radiohw {

bool begin(RadioState &st) {
  g_st = &st;
  Wire.setPins(SDA_PIN, SCL_PIN);  // I2C partilhado com o OLED

  radio.setup(RADIO_RESETPIN, SI4703_RST_PIN);  // RST (impulso LOW->HIGH no init)
  radio.setup(RADIO_SDAPIN, SDA_PIN);           // SDIO a LOW durante o reset
  // initWire associa o barramento (_i2cPort = &Wire) e corre o init();
  // faz o unico Wire.begin() do sistema.
  bool ok = radio.initWire(Wire);

  if (!ok) return false;  // sem chip: Wire ja ficou inicializado para o OLED

  radio.setBandFrequency(RADIO_BAND_FM, mhzToUnits(st.freq));
  radio.setMono(!st.stereo);
  radio.setVolume((int8_t)(st.volume * 15 / VOL_MAX));
  radio.setMute(true);  // o arranque e sempre em Bluetooth

  // RDS (lido diretamente do chip em poll(), ver pollSI4703RDS)
  rdsParser.init();
  rdsParser.attachServiceNameCallback(onServiceName);
  rdsParser.attachTextCallback(onRadioText);
  return ok;
}

void tune(const RadioState &st) {
  rdsParser.init();              // descarta RDS da estacao anterior
  if (g_st) { g_st->ps[0] = '\0'; g_st->rds[0] = '\0'; }
  radio.setFrequency(mhzToUnits(st.freq));
}

void prepareOutput(const RadioState &st) {
  radio.setMute(true);
  radio.setVolume((int8_t)(st.volume * 15 / VOL_MAX));
}

void setMuted(bool muted) { radio.setMute(muted); }

void applyVolume(const RadioState &st) {
  radio.setVolume((int8_t)(st.volume * 15 / VOL_MAX));
  radio.setMute(st.muted);
}

void applyMono(const RadioState &st) { radio.setMono(!st.stereo); }

void seekUp(RadioState &st) {
  radio.seekUp(true);
  st.freq = radio.getFrequency() / 100.0f;
  rdsParser.init();          // limpa RDS da estacao anterior
  st.ps[0] = '\0';
  st.rds[0] = '\0';
}

void poll(RadioState &st) {
  static unsigned long last = 0;
  if (millis() - last < 35) return;  // grupos RDS chegam ~a cada 87ms
  last = millis();
  pollSI4703RDS(st);
}

}  // namespace radiohw
