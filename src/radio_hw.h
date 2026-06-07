// radio_hw.h
// Camada de controlo do modulo FM Si4703 (biblioteca mathertel/Radio).
// Usada apenas com hardware real (ver DEMO_MODE em main.cpp).
#pragma once
#include "radio_state.h"

namespace radiohw {
bool begin(RadioState &st);          // reset + init + banda FM + freq/vol iniciais
void tune(const RadioState &st);     // aplica st.freq ao chip (limpa RDS antigo)
void applyVolume(const RadioState &st);  // aplica volume + mute
void applyMono(const RadioState &st);    // aplica modo mono/stereo
void seekUp(RadioState &st);         // procura automatica para cima
void poll(RadioState &st);           // periodico: RDS + RSSI/stereo -> estado
}  // namespace radiohw
