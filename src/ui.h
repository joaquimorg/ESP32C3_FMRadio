// ui.h
// Camada de desenho dos ecras do radio sobre o OLED SSD1306.
#pragma once

#include <Adafruit_SSD1306.h>
#include "radio_state.h"

namespace ui {

// Inicializa o display. Devolve false se o SSD1306 nao responder.
bool begin(Adafruit_SSD1306 &display);

// Ecra de apresentacao (splash). progress 0..1 anima a barra de carregamento.
void splash(Adafruit_SSD1306 &display, float progress);

// Desenha o ecra indicado pelo estado. Faz clearDisplay() + display().
void render(Adafruit_SSD1306 &display, Screen screen, const RadioState &st);

}  // namespace ui
