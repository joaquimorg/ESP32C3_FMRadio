// radio_hw.cpp
// Controlo do Si4703 + descodificacao RDS (PS e RadioText) para o estado.
#include "radio_hw.h"
#include "config.h"
#include <Wire.h>
#include <math.h>
#include <SI4703.h>
#include <RDSParser.h>

static SI4703 radio;
static RDSParser rdsParser;
static RadioState *g_st = nullptr;

// Frequencia em unidades de 10 kHz usadas pela biblioteca (101.20 MHz -> 10120).
static uint16_t mhzToUnits(float mhz) { return (uint16_t)lroundf(mhz * 100.0f); }

// Ponte: blocos RDS do chip -> parser RDS.
static void onRDS(uint16_t b1, uint16_t b2, uint16_t b3, uint16_t b4) {
  rdsParser.processData(b1, b2, b3, b4);
}

// Callback do parser: nome curto da estacao (PS).
static void onServiceName(const char *name) {
  if (!g_st) return;
  strncpy(g_st->ps, name, sizeof(g_st->ps) - 1);
  g_st->ps[sizeof(g_st->ps) - 1] = '\0';
  Serial.printf("PS: '%s'\n", g_st->ps);  // debug: nome RDS recebido
}

// Callback do parser: RadioText (texto longo).
static void onRadioText(const char *txt) {
  if (!g_st) return;
  strncpy(g_st->rds, txt, sizeof(g_st->rds) - 1);
  g_st->rds[sizeof(g_st->rds) - 1] = '\0';
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
  radio.setMute(st.muted);

  // RDS
  radio.attachReceiveRDS(onRDS);
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
  radio.checkRDS();  // tem protecao interna contra polling demasiado rapido

  static unsigned long last = 0;
  if (millis() - last > 120) {
    last = millis();
    RADIO_INFO info;
    radio.getRadioInfo(&info);
    st.stereo = info.stereo;

    // RSSI (0..~75) -> 0..5 barras por limiares (calibrado p/ Si4703).
    const uint8_t th[RSSI_BARS] = {8, 16, 26, 36, 44};
    uint8_t bars = 0;
    for (uint8_t i = 0; i < RSSI_BARS; i++)
      if (info.rssi >= th[i]) bars++;
    st.rssi = bars;
  }
}

}  // namespace radiohw
