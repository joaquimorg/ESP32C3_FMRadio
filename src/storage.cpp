// storage.cpp
// Implementacao da persistencia usando a biblioteca Preferences (NVS).
#include "storage.h"
#include <Preferences.h>

static Preferences prefs;
static const char *NS = "fmradio";  // namespace NVS (max 15 chars)

namespace storage {

void begin() {
  prefs.begin(NS, false);  // false = leitura/escrita
}

void loadPresets() {
  // So carrega se o tamanho gravado corresponder ao array atual.
  if (prefs.getBytesLength("presets") == sizeof(presets))
    prefs.getBytes("presets", presets, sizeof(presets));
}

void savePresets() {
  prefs.putBytes("presets", presets, sizeof(presets));
}

void loadSettings(RadioState &st) {
  st.freq = prefs.getFloat("freq", st.freq);
  st.volume = prefs.getUChar("vol", st.volume);
}

void saveSettings(const RadioState &st) {
  prefs.putFloat("freq", st.freq);
  prefs.putUChar("vol", st.volume);
}

}  // namespace storage
