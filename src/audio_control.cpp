// audio_control.cpp
#include "audio_control.h"
#include "config.h"
#include <Arduino.h>

namespace audioctrl {

void selectBluetoothAudio() {
  digitalWrite(PIN_AUDIO_SELECT, AUDIO_SELECT_BT_LEVEL);
}

void selectFmAudio() {
  digitalWrite(PIN_AUDIO_SELECT, AUDIO_SELECT_FM_LEVEL);
}

void enableFmAmplifier() {
  digitalWrite(PIN_AMP_CTRL, AMP_CTRL_ON_LEVEL);
}

void disableFmAmplifier() {
  digitalWrite(PIN_AMP_CTRL, AMP_CTRL_OFF_LEVEL);
}

void beginSafe() {
  // Pre-carrega os latches antes de ativar as saidas para evitar impulsos.
  selectBluetoothAudio();
  disableFmAmplifier();
  pinMode(PIN_AUDIO_SELECT, OUTPUT);
  pinMode(PIN_AMP_CTRL, OUTPUT);
  selectBluetoothAudio();
  disableFmAmplifier();
}

}  // namespace audioctrl
