// audio_control.h
// Controlo isolado do switch de audio e da logica OR do CTRL do amplificador.
#pragma once

namespace audioctrl {

// Deve ser a primeira inicializacao de hardware no setup().
void beginSafe();

void selectBluetoothAudio();
void selectFmAudio();
void enableFmAmplifier();
void disableFmAmplifier();

}  // namespace audioctrl
