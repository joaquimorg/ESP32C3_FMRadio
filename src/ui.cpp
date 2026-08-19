// ui.cpp
// Desenho dos ecras FM/Bluetooth sobre o SSD1306 128x64.
//
// Nota: a fonte por defeito da Adafruit GFX nao tem acentos, por isso o
// texto e escrito em ASCII (RADIO, OPCOES, ESTACAO, ...).
#include "ui.h"
#include "icons.h"
#include <math.h>

namespace ui {

// ---- Geometria comum -------------------------------------------------------
static constexpr int TOPBAR_Y   = 0;   // texto da barra de topo
static constexpr int TOPBAR_SEP = 11;  // linha sob a barra de topo
static constexpr int BTNBAR_SEP = 53;  // linha sobre a barra de botoes
static constexpr int BTNBAR_TXT = 56;  // texto da barra de botoes

// ---- Helpers de texto ------------------------------------------------------
static void printCentered(Adafruit_SSD1306 &d, const char *txt, int y,
                          uint8_t size) {
  int16_t bx, by;
  uint16_t bw, bh;
  d.setTextSize(size);
  d.getTextBounds(txt, 0, y, &bx, &by, &bw, &bh);
  d.setCursor((SCREEN_WIDTH - (int)bw) / 2 - bx, y);
  d.print(txt);
}

static void printRight(Adafruit_SSD1306 &d, const char *txt, int xRight, int y,
                       uint8_t size) {
  int16_t bx, by;
  uint16_t bw, bh;
  d.setTextSize(size);
  d.getTextBounds(txt, 0, y, &bx, &by, &bw, &bh);
  d.setCursor(xRight - (int)bw - bx, y);
  d.print(txt);
}

static void printCenteredIn(Adafruit_SSD1306 &d, const char *txt, int x0,
                            int x1, int y, uint8_t size) {
  int16_t bx, by;
  uint16_t bw, bh;
  d.setTextSize(size);
  d.getTextBounds(txt, 0, y, &bx, &by, &bw, &bh);
  d.setCursor(x0 + ((x1 - x0) - (int)bw) / 2 - bx, y);
  d.print(txt);
}

// ---- Componentes reutilizaveis --------------------------------------------

// Botao fisico premido (0=nenhum, 1..4), para feedback visual na barra.
static uint8_t s_pressedBtn = 0;

// Barra de botoes inferior: 4 celulas com as funcoes dos botoes fisicos.
// A celula do botao premido fica invertida.
static void drawButtonBar(Adafruit_SSD1306 &d, const char *l1, const char *l2,
                          const char *l3, const char *l4) {
  d.drawFastHLine(0, BTNBAR_SEP, SCREEN_WIDTH, SSD1306_WHITE);
  const char *labels[4] = {l1, l2, l3, l4};
  const int maxChars = 5;  // (32px celula / 6px) -> 5 chars cabem
  for (int i = 0; i < 4; i++) {
    int x0 = i * 32;
    bool pressed = (s_pressedBtn == i + 1);
    if (pressed)  // realce: celula a branco
      d.fillRect(x0, BTNBAR_SEP + 1, 32, SCREEN_HEIGHT - BTNBAR_SEP - 1,
                 SSD1306_WHITE);
    if (i > 0) d.drawFastVLine(x0, BTNBAR_SEP, SCREEN_HEIGHT - BTNBAR_SEP,
                               SSD1306_WHITE);
    // Trunca para garantir que nunca transborda para a celula vizinha.
    char buf[8];
    strncpy(buf, labels[i], maxChars);
    buf[maxChars] = '\0';
    d.setTextColor(pressed ? SSD1306_BLACK : SSD1306_WHITE);
    printCenteredIn(d, buf, x0, x0 + 32, BTNBAR_TXT, 1);
  }
  d.setTextColor(SSD1306_WHITE);
}

// Indicador de sinal: barras crescentes (preenchidas = bars).
static void drawRssi(Adafruit_SSD1306 &d, int x, int yBase, uint8_t bars) {
  for (int i = 0; i < RSSI_BARS; i++) {
    int h = 2 + i * 2;            // 2,4,6,8,10
    int bx = x + i * 3;           // passo compacto (3px) p/ caber na barra
    if (i < bars) {
      d.fillRect(bx, yBase - h, 2, h, SSD1306_WHITE);  // barra ativa cheia
    } else {
      d.fillRect(bx, yBase - 1, 2, 1, SSD1306_WHITE);  // inativa: so a base
    }
  }
}

// Barra de topo dos ecras de radio: "FM" + caixa STEREO/MONO + RSSI + label.
static void drawRadioTopBar(Adafruit_SSD1306 &d, const RadioState &st,
                            const char *rightLabel) {
  d.setTextSize(1);
  d.setCursor(0, TOPBAR_Y);
  d.print("FM");

  // Caixa STEREO / MONO (ancorada a esquerda; largura varia com o texto)
  const char *mode = st.stereo ? "STEREO" : "MONO";
  int boxX = 18, boxW = strlen(mode) * 6 + 5;
  d.drawRect(boxX, TOPBAR_Y - 1, boxW, 10, SSD1306_WHITE);
  d.setCursor(boxX + 3, TOPBAR_Y);
  d.print(mode);

  // Barras de sinal em posicao FIXA (nao dependem da largura da caixa).
  drawRssi(d, 88, TOPBAR_Y + 10, st.rssi);

  // Label a direita (P03 / TUNE / SCAN)
  printRight(d, rightLabel, SCREEN_WIDTH, TOPBAR_Y, 1);

  d.drawFastHLine(0, TOPBAR_SEP, SCREEN_WIDTH, SSD1306_WHITE);
}

// Estado persistente de um ticker (fila de textos a percorrer + posicao do
// scroll). Cada linha com scroll (ecra principal, protetor) tem a sua propria
// instancia, para nao misturar conteudos entre ecras diferentes.
struct ScrollState {
  String        q[4];        // fila FIFO de textos por mostrar
  uint8_t       qlen = 0;
  float         scrollX = 0;  // posicao atual do scroll, em px
  unsigned long lastMs = 0;
  bool          scrolling = false;  // resultado da ultima chamada
};

// Verifica se "newT" e uma continuacao valida de "oldT": todos os caracteres
// nao-espaco de oldT tem de aparecer iguais em newT na mesma posicao (com
// sinal fraco, o RDS preenche por posicao - segmentos ainda nao confirmados
// ficam como espaco e podem ser preenchidos mais tarde, fora de ordem).
static bool rdsTextCompatible(const String &oldT, const String &newT) {
  if (newT.length() < oldT.length()) return false;
  for (unsigned i = 0; i < oldT.length(); i++) {
    char o = oldT[i];
    if (o != ' ' && o != newT[i]) return false;
  }
  return true;
}

// Texto com scroll horizontal quando excede a largura da janela. Quando o
// texto muda a meio do scroll, o texto novo e adicionado ao FIM da fila em
// vez de substituir o atual - a mensagem antiga continua a deslizar e so e
// descartada quando ja saiu completamente de vista, sem nenhum salto visual.
static void drawScrollingText(Adafruit_SSD1306 &d, const char *txt, int y,
                              int x0, int w, ScrollState &s) {
  d.setTextSize(1);
  const int gap = 16;    // espaco (px) entre mensagens consecutivas
  const int charW = 6;

  // Enfileira o texto novo, se ainda nao for o ultimo pedido (evita repetir
  // o mesmo texto em cada frame enquanto a estacao nao o atualiza). O RDS
  // entrega o RadioText de forma progressiva e pode preencher buracos fora
  // de ordem - se o novo texto e uma continuacao compativel do que ja esta
  // na fila, e a MESMA mensagem a crescer: atualiza-se no lugar em vez de
  // empilhar uma entrada nova, para o scroll nao saltar a cada atualizacao.
  if (txt[0] != '\0' && (s.qlen == 0 || s.q[s.qlen - 1] != txt)) {
    if (s.qlen > 0 && rdsTextCompatible(s.q[s.qlen - 1], String(txt))) {
      s.q[s.qlen - 1] = txt;
    } else if (s.qlen < 4) {
      s.q[s.qlen++] = txt;
    }
  }

  if (s.qlen == 0) { s.scrolling = false; return; }

  int firstW = s.q[0].length() * charW;

  // So 1 mensagem em fila e cabe no espaco: sem scroll. Alinhado a esquerda
  // (nao centrado) porque o RDS entrega o texto progressivamente - centrar
  // faria o texto saltar de posicao a cada letra nova, enquanto cresce perto
  // da largura da janela.
  if (s.qlen == 1 && firstW <= w) {
    d.setCursor(x0, y);
    d.print(s.q[0]);
    s.scrollX = 0;
    s.lastMs = 0;
    s.scrolling = false;
    return;
  }
  s.scrolling = true;

  // Avanca o scroll a velocidade constante (independente da framerate).
  unsigned long now = millis();
  if (s.lastMs == 0) s.lastMs = now;
  float dt = (now - s.lastMs) / 1000.0f;
  if (dt > 0.2f) dt = 0.2f;  // evita saltos apos uma pausa longa (troca de ecra)
  s.lastMs = now;
  s.scrollX += dt * 20.0f;  // ~1px/50ms, igual a velocidade original

  // Quando a 1a mensagem sai completamente de vista: se ja ha outra a
  // seguir, descarta-a e continua o scroll a partir da proxima (sem salto,
  // porque so removemos o que ja nao era visivel); senao, mantem-na (loop).
  int firstPeriod = firstW + gap;
  while (s.scrollX >= firstPeriod) {
    s.scrollX -= firstPeriod;
    if (s.qlen > 1) {
      for (uint8_t i = 1; i < s.qlen; i++) s.q[i - 1] = s.q[i];
      s.qlen--;
    }
    firstW = s.q[0].length() * charW;
    firstPeriod = firstW + gap;
  }

  // Desenha a fila em sequencia; se so houver 1 mensagem, desenha uma copia
  // extra a seguir para o loop ficar continuo (mesmo truque de sempre).
  int cum = 0;
  for (uint8_t i = 0; i < s.qlen; i++) {
    d.setCursor(x0 - (int)s.scrollX + cum, y);
    d.print(s.q[i]);
    cum += (int)s.q[i].length() * charW + gap;
  }
  if (s.qlen == 1) {
    d.setCursor(x0 - (int)s.scrollX + cum, y);
    d.print(s.q[0]);
  }
}

// Frequencia em grande + "MHz", opcionalmente com setas laterais.
static void drawBigFreq(Adafruit_SSD1306 &d, float freq, bool arrows,
                        int numY = 14) {
  char buf[8];
  snprintf(buf, sizeof(buf), "%.2f", freq);

  const int numSize = 3;
  int numW = (int)strlen(buf) * 6 * numSize;

  if (arrows) {
    // Modo sintonia: numero centrado a toda a largura, setas nas margens,
    // sem "MHz" (nao ha espaco para freq de 6 digitos + setas + sufixo).
    int x0 = (SCREEN_WIDTH - numW) / 2;
    if (x0 < 7) x0 = 7;
    d.setTextSize(numSize);
    d.setCursor(x0, numY);
    d.print(buf);

    int cy = numY + (7 * numSize) / 2;
    d.fillTriangle(0, cy, 6, cy - 5, 6, cy + 5, SSD1306_WHITE);
    d.fillTriangle(SCREEN_WIDTH - 1, cy, SCREEN_WIDTH - 7, cy - 5,
                   SCREEN_WIDTH - 7, cy + 5, SSD1306_WHITE);
    return;
  }

  // Modo normal: numero + "MHz", o conjunto centrado.
  const int gap = 2;
  int mhzW = 3 * 6;  // "MHz" size 1
  int totalW = numW + gap + mhzW;
  int x0 = (SCREEN_WIDTH - totalW) / 2;
  if (x0 < 0) x0 = 0;

  d.setTextSize(numSize);
  d.setCursor(x0, numY);
  d.print(buf);

  d.setTextSize(1);
  d.setCursor(x0 + numW + gap, numY + (7 * numSize) - 8);
  d.print("MHz");
}

// Regua da banda FM (88..108) com marcas e ponteiro na frequencia atual.
// withLabels desenha os numeros (88..108); yLine define a posicao da linha.
static void drawBand(Adafruit_SSD1306 &d, float freq, bool withLabels = true,
                     int yLine = 49) {
  const int xL = 6, xR = SCREEN_WIDTH - 6;
  const int labelY = yLine - 13;

  // Etiquetas das marcas principais
  const int marks[] = {88, 92, 96, 100, 104, 108};
  d.setTextSize(1);
  for (int m : marks) {
    int x = xL + (int)((float)(m - (int)FM_MIN) / (FM_MAX - FM_MIN) *
                       (xR - xL));
    if (withLabels) {
      char lbl[5];
      snprintf(lbl, sizeof(lbl), "%d", m);
      int w = strlen(lbl) * 6;
      int lx = x - w / 2;
      if (lx < 0) lx = 0;
      if (lx + w > SCREEN_WIDTH) lx = SCREEN_WIDTH - w;
      d.setCursor(lx, labelY);
      d.print(lbl);
    }
    d.drawFastVLine(x, yLine - 2, 4, SSD1306_WHITE);  // marca principal
  }

  // Linha base + marcas menores
  d.drawFastHLine(xL, yLine, xR - xL, SSD1306_WHITE);
  for (int m = (int)FM_MIN; m <= (int)FM_MAX; m += 1) {
    int x = xL + (int)((float)(m - (int)FM_MIN) / (FM_MAX - FM_MIN) *
                       (xR - xL));
    d.drawFastVLine(x, yLine - 1, 2, SSD1306_WHITE);
  }

  // Ponteiro (triangulo a apontar para baixo) na frequencia atual
  float f = freq;
  if (f < FM_MIN) f = FM_MIN;
  if (f > FM_MAX) f = FM_MAX;
  int px = xL + (int)((f - FM_MIN) / (FM_MAX - FM_MIN) * (xR - xL));
  d.fillTriangle(px, yLine - 1, px - 3, yLine - 6, px + 3, yLine - 6,
                 SSD1306_WHITE);
}

// ---- Icones (bitmaps 16x16) -----------------------------------------------
// Desenha um icone 16x16 centrado em (cx, cy) com a cor indicada.
static void drawIcon(Adafruit_SSD1306 &d, const unsigned char *bmp, int cx,
                     int cy, uint16_t color = SSD1306_WHITE) {
  d.drawBitmap(cx - 8, cy - 8, bmp, 16, 16, color);
}

// Simbolo Bluetooth compacto, desenhado em vetor para nao criar outro bitmap.
static void drawBluetoothIcon(Adafruit_SSD1306 &d, int cx, int cy,
                              uint16_t color) {
  d.drawLine(cx, cy - 8, cx, cy + 8, color);
  d.drawLine(cx, cy - 8, cx + 6, cy - 3, color);
  d.drawLine(cx + 6, cy - 3, cx - 5, cy + 5, color);
  d.drawLine(cx - 5, cy - 5, cx + 6, cy + 3, color);
  d.drawLine(cx + 6, cy + 3, cx, cy + 8, color);
}

// ===========================================================================
//  ECRAS
// ===========================================================================

// 1. Ecra principal (radio em reproducao)
static void drawMain(Adafruit_SSD1306 &d, const RadioState &st) {
  char pLabel[5] = "--";
  if (st.currentPreset >= 0)
    snprintf(pLabel, sizeof(pLabel), "P%02d", st.currentPreset + 1);
  drawRadioTopBar(d, st, pLabel);

  if (st.ps[0] != '\0') {
    // Com RDS: nome da estacao em destaque + frequencia pequena por baixo.
    printCentered(d, st.ps, 16, 2);
    char f[16];
    snprintf(f, sizeof(f), "%.2f MHz", st.freq);
    printCentered(d, f, 34, 1);
  } else {
    // Sem RDS: so a frequencia em grande.
    drawBigFreq(d, st.freq, false, 18);
  }

  // Linha de RDS RadioText com scroll, no espaco antes da barra de botoes.
  // Sem RDS, deixa o espaco em branco em vez de mostrar um texto de preenchimento.
  static ScrollState mainRdsScroll;
  if (st.rds[0] != '\0') drawScrollingText(d, st.rds, 44, 0, SCREEN_WIDTH, mainRdsScroll);

  drawButtonBar(d, "1 -", "2 +", "3 VOL", "4 PRE");
}

// 2. Sintonia / ajuste de frequencia
static void drawTune(Adafruit_SSD1306 &d, const RadioState &st) {
  drawRadioTopBar(d, st, "TUNE");
  drawBigFreq(d, st.freq, true);
  drawBand(d, st.freq);
  drawButtonBar(d, "1 -", "2 +", "3 PAS", "4 SAI");
}

// 3. Modo volume
static void drawVolume(Adafruit_SSD1306 &d, const RadioState &st) {
  d.setTextSize(1);
  d.setCursor(0, TOPBAR_Y);
  d.print("VOLUME");
  printRight(d, st.stereo ? "STEREO" : "MONO", SCREEN_WIDTH, TOPBAR_Y, 1);
  d.drawFastHLine(0, TOPBAR_SEP, SCREEN_WIDTH, SSD1306_WHITE);

  drawIcon(d, ic_speaker, 12, 24);
  if (st.muted) d.drawLine(4, 16, 20, 32, SSD1306_WHITE);  // risco de mute

  // Valor grande
  char buf[6];
  if (st.muted) snprintf(buf, sizeof(buf), "MUTE");
  else snprintf(buf, sizeof(buf), "%d", st.volume);
  d.setTextSize(3);
  printCenteredIn(d, buf, 30, SCREEN_WIDTH, 16, 3);

  // Barra de segmentos 0..VOL_MAX
  const int segs = 15;
  const int x0 = 18, y0 = 42, sw = 5, gap = 1, sh = 8;
  int filled = (int)roundf((float)st.volume / VOL_MAX * segs);
  if (st.muted) filled = 0;
  for (int i = 0; i < segs; i++) {
    int bx = x0 + i * (sw + gap);
    if (i < filled) d.fillRect(bx, y0, sw, sh, SSD1306_WHITE);
    else d.drawRect(bx, y0, sw, sh, SSD1306_WHITE);
  }
  d.setTextSize(1);
  d.setCursor(6, y0 + 1);
  d.print("0");
  printRight(d, "30", SCREEN_WIDTH, y0 + 1, 1);

  drawButtonBar(d, "1 -", "2 +", "3 MUT", "4 OK");
}

// 4. Presets (favoritos) - lista vertical, 4 por pagina.
static void drawPresets(Adafruit_SSD1306 &d, const RadioState &st) {
  d.setTextSize(1);
  printCentered(d, "PRESETS", TOPBAR_Y, 1);

  // Recolhe so as memorias preenchidas (esconde as vazias).
  uint8_t valid[PRESET_COUNT];
  int n = 0;
  for (uint8_t i = 0; i < PRESET_COUNT; i++)
    if (presets[i].freq > 0.0f) valid[n++] = i;

  if (n == 0) {  // nenhuma memoria gravada
    d.drawFastHLine(0, TOPBAR_SEP, SCREEN_WIDTH, SSD1306_WHITE);
    printCentered(d, "(sem memorias)", 28, 1);
    drawButtonBar(d, "1 \x18", "2 \x19", "3 OK", "4 SAI");
    return;
  }

  // Posicao do cursor dentro da lista de validas.
  int cpos = 0;
  for (int k = 0; k < n; k++)
    if (valid[k] == st.presetCursor) cpos = k;

  int pages = (n + 3) / 4;
  int page = cpos / 4;
  char pag[10];
  snprintf(pag, sizeof(pag), "PG %d/%d", page + 1, pages);
  printRight(d, pag, SCREEN_WIDTH, TOPBAR_Y, 1);
  d.drawFastHLine(0, TOPBAR_SEP, SCREEN_WIDTH, SSD1306_WHITE);

  const int rowH = 10;
  for (int i = 0; i < 4; i++) {
    int k = page * 4 + i;
    if (k >= n) break;
    int idx = valid[k];
    int rowY = 14 + i * rowH;
    bool sel = (idx == st.presetCursor);
    if (sel) {  // linha selecionada: barra invertida
      d.fillRect(0, rowY - 1, SCREEN_WIDTH, rowH, SSD1306_WHITE);
      d.setTextColor(SSD1306_BLACK);
    } else {
      d.setTextColor(SSD1306_WHITE);
    }
    char tmp[16];
    snprintf(tmp, sizeof(tmp), "P%02d", idx + 1);  // coluna numero
    d.setCursor(3, rowY);
    d.print(tmp);
    snprintf(tmp, sizeof(tmp), "%.2f", presets[idx].freq);  // coluna freq
    d.setCursor(28, rowY);
    d.print(tmp);
    strncpy(tmp, presets[idx].name, 10);  // coluna nome (ate 10 chars)
    tmp[10] = '\0';
    d.setCursor(66, rowY);
    d.print(tmp);
  }
  d.setTextColor(SSD1306_WHITE);
  drawButtonBar(d, "1 \x18", "2 \x19", "3 OK", "4 SAI");  // setas cima/baixo
}

// 5. Scan (procura automatica)
static void drawScan(Adafruit_SSD1306 &d, const RadioState &st) {
  drawRadioTopBar(d, st, "SCAN");
  drawBigFreq(d, st.freq, false, 13);            // freq 13..34

  if (st.autoScan) {                             // modo automatico
    char t[24];
    snprintf(t, sizeof(t), "AUTO -> P%02d", st.scanSlot + 1);
    printCentered(d, t, 36, 1);
  } else {
    printCentered(d, "A PROCURAR ESTACAO...", 36, 1);
  }

  // mini regua: ponteiro (45..49) + linha (50), sem colidir com o texto
  const int xL = 6, xR = SCREEN_WIDTH - 6, yLine = 50;
  d.drawFastHLine(xL, yLine, xR - xL, SSD1306_WHITE);
  float f = st.freq;
  int px = xL + (int)((f - FM_MIN) / (FM_MAX - FM_MIN) * (xR - xL));
  d.fillTriangle(px, yLine - 1, px - 3, yLine - 5, px + 3, yLine - 5,
                 SSD1306_WHITE);

  drawButtonBar(d, "1 STP", "2 >>", st.autoScan ? "3 PARA" : "3 AUTO", "4 SAI");
}

// 6. Menu principal
static void drawMenu(Adafruit_SSD1306 &d, const RadioState &st) {
  const char *labels[6] = {"RADIO", "PRESETS", "VOLUME", "SCAN",
                           "BLUETOOTH", "SOBRE"};
  const int n = 6;
  const int cellW = SCREEN_WIDTH / n;
  const int iconCY = 22;
  for (int i = 0; i < n; i++) {
    int cx = i * cellW + cellW / 2;
    bool sel = (i == st.menuCursor);
    const unsigned char *icons[6] = {ic_radio, ic_star, ic_speaker, ic_search,
                                     nullptr, ic_info};
    if (sel) {  // selecionado: caixa a branco com o icone invertido (preto)
      d.fillRoundRect(i * cellW + 1, 4, cellW - 2, 32, 3, SSD1306_WHITE);
      if (i == 4) drawBluetoothIcon(d, cx, iconCY, SSD1306_BLACK);
      else drawIcon(d, icons[i], cx, iconCY, SSD1306_BLACK);
    } else {
      if (i == 4) drawBluetoothIcon(d, cx, iconCY, SSD1306_WHITE);
      else drawIcon(d, icons[i], cx, iconCY, SSD1306_WHITE);
    }
  }
  // So a legenda do item selecionado, centrada (evita sobreposicao de textos).
  printCentered(d, labels[st.menuCursor], 42, 1);
  drawButtonBar(d, "1 <", "2 >", "3 OK", "4 SAI");
}

// 7. Mensagem / informacao
static void drawMessage(Adafruit_SSD1306 &d, const RadioState &st) {
  // Texto a toda a largura (centrado) em linhas proprias.
  printCentered(d, st.msgTitle, 1, 1);
  printCentered(d, st.msgLine, 14, 1);
  printCentered(d, st.msgFoot, 26, 1);
  // Icones numa linha propria em baixo, a flanquear (sem colidir com o texto).
  drawIcon(d, ic_info, 10, 43);
  drawIcon(d, ic_check, SCREEN_WIDTH - 11, 43);
  drawButtonBar(d, "1 <", "2 >", "3 OK", "4 SAI");
}

// Protecao de ecra: nome + frequencia (ou "BLUETOOTH" fora do FM) a derivar
// lentamente (anti burn-in), com o RDS a fazer scroll no fundo.
// Reposicoes suaves para novos pontos.
static void drawSaver(Adafruit_SSD1306 &d, const RadioState &st) {
  bool isFM = (st.audioMode == AudioMode::FM);

  char freq[12];
  if (isFM) snprintf(freq, sizeof(freq), "%.2f", st.freq);  // sem "MHz" p/ ser estreito
  else strcpy(freq, "BLUETOOTH");
  bool hasName = isFM && (st.ps[0] != '\0');
  int nameW = hasName ? (int)strlen(st.ps) * 6 : 0;
  int freqW = (int)strlen(freq) * 12;             // size 2
  int blockW = (nameW > freqW) ? nameW : freqW;
  int blockH = hasName ? (8 + 2 + 16) : 16;

  const int rdsY = 56;
  int xMax = SCREEN_WIDTH - blockW; if (xMax < 0) xMax = 0;
  int yMax = rdsY - blockH - 2;     if (yMax < 0) yMax = 0;

  static float bx = 4, by = 2;   // posicao atual
  static float tx = 4, ty = 2;   // posicao alvo
  static unsigned long arriveT = 0;

  bx += (tx - bx) * 0.05f;       // easing suave ate ao alvo
  by += (ty - by) * 0.05f;
  unsigned long now = millis();
  if (fabsf(tx - bx) < 0.6f && fabsf(ty - by) < 0.6f) {
    if (arriveT == 0) arriveT = now;
    if (now - arriveT > 3000) {  // 3s parado, escolhe novo ponto
      tx = random(0, xMax + 1);
      ty = random(0, yMax + 1);
      arriveT = 0;
    }
  } else {
    arriveT = 0;
  }

  int ix = (int)(bx + 0.5f), iy = (int)(by + 0.5f);
  if (hasName) {
    printCenteredIn(d, st.ps, ix, ix + blockW, iy, 1);
    d.setTextSize(2);
    d.setCursor(ix + (blockW - freqW) / 2, iy + 10);
    d.print(freq);
  } else {
    d.setTextSize(2);
    d.setCursor(ix, iy);
    d.print(freq);
  }

  // Sem RDS, deixa o espaco em branco em vez de mostrar um texto de preenchimento.
  // Quando o texto cabe no ecra (ex.: "IKEA KALLSUP") fica parado, sem scroll
  // nem deriva - o bloco nome/frequencia acima ja e responsavel pela deriva
  // anti burn-in desta zona do ecra.
  static ScrollState saverRdsScroll;
  const char *rds = isFM ? st.rds : "IKEA KALLSUP";
  if (rds[0] != '\0') drawScrollingText(d, rds, rdsY, 0, SCREEN_WIDTH, saverRdsScroll);
}

// Fonte Bluetooth original do KALLSUP. O ESP32 nao tenta inferir pairing,
// volume ou metadata porque nao comunica com o JL7016C8.
static void drawBluetooth(Adafruit_SSD1306 &d) {
  printCentered(d, "BLUETOOTH", 15, 2);
  printCentered(d, "IKEA KALLSUP", 38, 1);
  drawButtonBar(d, "FM", "", "", "");
}

// Ecra de apresentacao (splash) com barra de carregamento animada.
void splash(Adafruit_SSD1306 &d, float progress) {
  if (progress < 0) progress = 0;
  if (progress > 1) progress = 1;
  d.clearDisplay();
  d.setTextColor(SSD1306_WHITE);
  d.setTextWrap(false);

  d.drawRoundRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 5, SSD1306_WHITE);
  drawIcon(d, ic_radio, SCREEN_WIDTH / 2, 16);     // icone do radio
  printCentered(d, "FM RADIO", 26, 2);             // titulo
  printCentered(d, "ESP32-C3 + Si4703", 44, 1);    // subtitulo

  // Barra de carregamento
  const int bx = 14, by = 55, bw = SCREEN_WIDTH - 28, bh = 5;
  d.drawRect(bx, by, bw, bh, SSD1306_WHITE);
  int fill = (int)((bw - 2) * progress);
  if (fill > 0) d.fillRect(bx + 1, by + 1, fill, bh - 2, SSD1306_WHITE);

  d.display();
}

// ===========================================================================
bool begin(Adafruit_SSD1306 &display) {
  // Nota: o barramento Wire e inicializado uma unica vez no main (pelo radio
  // ou pelo setup), para evitar um segundo Wire.begin() que faz crash no C3.
  if (!display.begin(SSD1306_SWITCHCAPVCC, OLED_ADDR)) return false;
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.display();
  return true;
}

void render(Adafruit_SSD1306 &display, Screen screen, const RadioState &st) {
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);  // o RDS faz scroll, nunca quebra de linha
  display.setTextSize(1);
  s_pressedBtn = st.pressedBtn;  // feedback do botao premido na barra
  switch (screen) {
    case SCREEN_MAIN:    drawMain(display, st); break;
    case SCREEN_TUNE:    drawTune(display, st); break;
    case SCREEN_VOLUME:  drawVolume(display, st); break;
    case SCREEN_PRESETS: drawPresets(display, st); break;
    case SCREEN_SCAN:    drawScan(display, st); break;
    case SCREEN_MENU:    drawMenu(display, st); break;
    case SCREEN_MESSAGE: drawMessage(display, st); break;
    case SCREEN_SAVER:   drawSaver(display, st); break;
    case SCREEN_BLUETOOTH: drawBluetooth(display); break;
  }
  display.display();
}

}  // namespace ui
