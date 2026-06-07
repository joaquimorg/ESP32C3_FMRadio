// storage.h
// Persistencia das memorias (presets) e definicoes na flash (NVS/Preferences).
#pragma once
#include "radio_state.h"

namespace storage {
void begin();                          // abre o namespace NVS
void loadPresets();                    // le presets[] da NVS (se existirem)
void savePresets();                    // grava presets[] na NVS
void loadSettings(RadioState &st);     // le ultima frequencia/volume
void saveSettings(const RadioState &st);
}  // namespace storage
