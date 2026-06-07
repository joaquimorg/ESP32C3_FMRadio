from pathlib import Path
from base64 import b64encode
import re
from xml.sax.saxutils import escape


ROOT = Path(__file__).resolve().parents[1]
DOCS = ROOT / "docs"
SCREENS = DOCS / "screens"
ICONS_H = ROOT / "src" / "icons.h"


def write(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8")


def svg_text(x, y, text, size=10, anchor="start", fill="#e9f7ff", weight="400"):
    return (
        f'<text x="{x}" y="{y}" font-family="Consolas,monospace" '
        f'font-size="{size}" font-weight="{weight}" text-anchor="{anchor}" '
        f'fill="{fill}">{escape(text)}</text>'
    )


def load_icons():
    text = ICONS_H.read_text(encoding="utf-8")
    icons = {}
    pattern = re.compile(r"static const unsigned char PROGMEM (ic_\w+)\[\] = \{(.*?)\};", re.S)
    for name, body in pattern.findall(text):
        values = [int(value, 16) for value in re.findall(r"0x[0-9A-Fa-f]{2}", body)]
        if len(values) != 32:
            raise ValueError(f"{name} should have 32 bytes, got {len(values)}")
        icons[name] = values
    return icons


ICONS = load_icons()


def icon(name, cx, cy, fill="#dff7ff", pixel=1, invert=False):
    data = ICONS[name]
    x0 = cx - 8 * pixel
    y0 = cy - 8 * pixel
    parts = []
    for y in range(16):
        row = (data[y * 2] << 8) | data[y * 2 + 1]
        for x in range(16):
            bit = bool(row & (0x8000 >> x))
            if bit ^ invert:
                parts.append(
                    f'<rect x="{x0 + x * pixel}" y="{y0 + y * pixel}" '
                    f'width="{pixel}" height="{pixel}" fill="{fill}"/>'
                )
    return "\n".join(parts)


def screen_svg(title: str, elements: list[str]) -> str:
    scale = 4
    w, h = 128 * scale, 64 * scale
    body = "\n".join(elements)
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 128 64">
  <title>{escape(title)}</title>
  <rect width="128" height="64" rx="4" fill="#050b12"/>
  <rect x="0.5" y="0.5" width="127" height="63" rx="4" fill="none" stroke="#dff7ff"/>
  {body}
</svg>
"""


def button_bar(labels):
    parts = ['<line x1="0" y1="53" x2="128" y2="53" stroke="#dff7ff" stroke-width="1"/>']
    for i, label in enumerate(labels):
        x = i * 32
        if i:
            parts.append(f'<line x1="{x}" y1="53" x2="{x}" y2="64" stroke="#dff7ff" stroke-width="1"/>')
        parts.append(svg_text(x + 16, 62, label, 7, "middle"))
    return parts


def rssi(x, y, bars):
    parts = []
    for i in range(5):
        h = 2 + i * 2
        bx = x + i * 3
        if i < bars:
            parts.append(f'<rect x="{bx}" y="{y-h}" width="2" height="{h}" fill="#dff7ff"/>')
        else:
            parts.append(f'<rect x="{bx}" y="{y-1}" width="2" height="1" fill="#dff7ff"/>')
    return parts


def topbar(mode, bars, label):
    parts = [
        svg_text(1, 8, "FM", 8),
        f'<rect x="18" y="0.5" width="{len(mode) * 6 + 5}" height="10" fill="none" stroke="#dff7ff"/>',
        svg_text(21, 8, mode, 7),
        svg_text(127, 8, label, 7, "end"),
        '<line x1="0" y1="11" x2="128" y2="11" stroke="#dff7ff" stroke-width="1"/>',
    ]
    parts.extend(rssi(88, 10, bars))
    return parts


def draw_band(freq=98.7, y=49, labels=True):
    x_l, x_r = 6, 122
    parts = [f'<line x1="{x_l}" y1="{y}" x2="{x_r}" y2="{y}" stroke="#dff7ff"/>']
    for mark in [88, 92, 96, 100, 104, 108]:
        x = x_l + (mark - 88) / 20 * (x_r - x_l)
        parts.append(f'<line x1="{x:.1f}" y1="{y-2}" x2="{x:.1f}" y2="{y+2}" stroke="#dff7ff"/>')
        if labels:
            parts.append(svg_text(x, y - 7, str(mark), 7, "middle"))
    px = x_l + (freq - 88) / 20 * (x_r - x_l)
    parts.append(f'<polygon points="{px:.1f},{y-1} {px-3:.1f},{y-6} {px+3:.1f},{y-6}" fill="#dff7ff"/>')
    return parts


def volume_segments(filled=7):
    parts = []
    for i in range(15):
        x = 18 + i * 6
        if i < filled:
            parts.append(f'<rect x="{x}" y="42" width="5" height="8" fill="#dff7ff"/>')
        else:
            parts.append(f'<rect x="{x}" y="42" width="5" height="8" fill="none" stroke="#dff7ff"/>')
    return parts


def make_screens():
    screens = {
        "splash.svg": screen_svg("Apresentacao", [
            icon("ic_radio", 64, 16),
            svg_text(64, 32, "FM RADIO", 14, "middle", weight="700"),
            svg_text(64, 46, "ESP32-C3 + Si4703", 7, "middle"),
            '<rect x="14" y="55" width="100" height="5" fill="none" stroke="#dff7ff"/>',
            '<rect x="15" y="56" width="54" height="3" fill="#dff7ff"/>',
        ]),
        "01-principal-rds.svg": screen_svg("Principal com RDS", [
            *topbar("STEREO", 3, "P03"),
            svg_text(64, 29, "Comercial", 13, "middle", weight="700"),
            svg_text(64, 41, "97.40 MHz", 8, "middle"),
            svg_text(2, 51, "< As melhores musicas sem ...", 7),
            *button_bar(["1 -", "2 +", "3 VOL", "4 PRE"]),
        ]),
        "01-principal-sem-rds.svg": screen_svg("Principal sem RDS", [
            *topbar("STEREO", 2, "--"),
            svg_text(64, 34, "97.40", 20, "middle", weight="700"),
            svg_text(101, 34, "MHz", 7),
            svg_text(2, 50, "< RDS RadioText em scroll ...", 7),
            *button_bar(["1 -", "2 +", "3 VOL", "4 PRE"]),
        ]),
        "02-sintonia.svg": screen_svg("Sintonia", [
            *topbar("MONO", 2, "TUNE"),
            svg_text(64, 31, "98.70", 18, "middle", weight="700"),
            '<polygon points="1,25 7,20 7,30" fill="#dff7ff"/>',
            '<polygon points="127,25 121,20 121,30" fill="#dff7ff"/>',
            *draw_band(98.7),
            *button_bar(["1 -", "2 +", "3 PAS", "4 SAI"]),
        ]),
        "03-volume.svg": screen_svg("Volume", [
            svg_text(1, 8, "VOLUME", 8),
            svg_text(127, 8, "STEREO", 8, "end"),
            '<line x1="0" y1="11" x2="128" y2="11" stroke="#dff7ff"/>',
            icon("ic_speaker", 12, 24),
            svg_text(64, 34, "14", 20, "middle", weight="700"),
            svg_text(7, 49, "0", 7),
            *volume_segments(7),
            svg_text(122, 49, "30", 7, "end"),
            *button_bar(["1 -", "2 +", "3 MUT", "4 OK"]),
        ]),
        "04-presets.svg": screen_svg("Presets", [
            svg_text(64, 8, "PRESETS", 8, "middle", weight="700"),
            svg_text(127, 8, "PG 1/2", 7, "end"),
            '<line x1="0" y1="11" x2="128" y2="11" stroke="#dff7ff"/>',
            svg_text(3, 22, "P01", 8), svg_text(29, 22, "89.50", 8), svg_text(67, 22, "RFM", 8),
            '<rect x="0" y="24" width="128" height="10" fill="#dff7ff"/>',
            svg_text(3, 32, "P02", 8, fill="#050b12"), svg_text(29, 32, "97.40", 8, fill="#050b12"), svg_text(67, 32, "Comercial", 8, fill="#050b12"),
            svg_text(3, 42, "P03", 8), svg_text(29, 42, "95.70", 8), svg_text(67, 42, "Antena 1", 8),
            svg_text(3, 52, "P05", 8), svg_text(29, 52, "104.30", 8), svg_text(67, 52, "M80", 8),
            *button_bar(["1 ^", "2 v", "3 OK", "4 SAI"]),
        ]),
        "05-scan-auto.svg": screen_svg("Scan AUTO", [
            *topbar("STEREO", 4, "SCAN"),
            svg_text(64, 32, "101.20", 18, "middle", weight="700"),
            svg_text(64, 43, "AUTO -> P04", 8, "middle"),
            *draw_band(101.2, 50, False),
            *button_bar(["1 STP", "2 >>", "3 PARA", "4 SAI"]),
        ]),
        "06-menu.svg": screen_svg("Menu", [
            '<rect x="76" y="4" width="23" height="32" rx="3" fill="#dff7ff"/>',
            icon("ic_radio", 12, 22),
            icon("ic_star", 37, 22),
            icon("ic_speaker", 62, 22),
            icon("ic_search", 87, 22, fill="#050b12"),
            icon("ic_info", 112, 22),
            svg_text(64, 49, "SCAN", 8, "middle", weight="700"),
            *button_bar(["1 <", "2 >", "3 OK", "4 SAI"]),
        ]),
        "07-mensagem.svg": screen_svg("Mensagem", [
            svg_text(64, 9, "ESTACAO GUARDADA", 8, "middle", weight="700"),
            svg_text(64, 22, "RFM (89.50 MHz)", 8, "middle"),
            svg_text(64, 34, "no PRESET 01", 8, "middle"),
            icon("ic_info", 10, 43),
            icon("ic_check", 117, 43),
            *button_bar(["1 <", "2 >", "3 OK", "4 SAI"]),
        ]),
        "08-protecao-ecra.svg": screen_svg("Protecao de ecra", [
            svg_text(64, 28, "Comercial", 9, "middle"),
            svg_text(64, 42, "97.40", 14, "middle", weight="700"),
            svg_text(2, 60, "< RadioText em scroll ...", 7),
        ]),
    }
    for name, content in screens.items():
        write(SCREENS / name, content)


def make_wiring():
    def box(x, y, w, h, title, lines, fill="#101a24"):
        out = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{fill}" stroke="#26394a"/>']
        out.append(svg_text(x + w / 2, y + 22, title, 18, "middle", "#ffffff", "700"))
        for i, line in enumerate(lines):
            out.append(svg_text(x + 18, y + 52 + i * 24, line, 15, "start", "#d7e7ef"))
        return out

    wires = [
        ('<path d="M250 120 C330 120 330 96 410 96" stroke="#28c76f" stroke-width="4" fill="none"/>', "SDA GPIO3"),
        ('<path d="M250 150 C330 150 330 126 410 126" stroke="#00c2ff" stroke-width="4" fill="none"/>', "SCL GPIO4"),
        ('<path d="M250 180 C330 180 330 156 410 156" stroke="#ffcf5a" stroke-width="4" fill="none"/>', "3V3"),
        ('<path d="M250 210 C330 210 330 186 410 186" stroke="#9aa7b0" stroke-width="4" fill="none"/>', "GND"),
        ('<path d="M250 260 C335 260 335 300 410 300" stroke="#28c76f" stroke-width="4" fill="none"/>', "SDA GPIO3"),
        ('<path d="M250 290 C335 290 335 330 410 330" stroke="#00c2ff" stroke-width="4" fill="none"/>', "SCL GPIO4"),
        ('<path d="M250 320 C335 320 335 360 410 360" stroke="#ff6b6b" stroke-width="4" fill="none"/>', "RST GPIO20"),
        ('<path d="M250 350 C335 350 335 390 410 390" stroke="#ffcf5a" stroke-width="4" fill="none"/>', "3V3"),
        ('<path d="M250 380 C335 380 335 420 410 420" stroke="#9aa7b0" stroke-width="4" fill="none"/>', "GND"),
    ]
    button_paths = []
    for i, gpio in enumerate([5, 6, 7, 8]):
        y = 500 + i * 46
        button_paths.append(f'<path d="M250 {y} H500" stroke="#c792ea" stroke-width="4" fill="none"/>')
        button_paths.append(svg_text(255, y - 8, f"BTN{i+1}: GPIO{gpio} -> botao -> GND (INPUT_PULLUP)", 14, fill="#f4e7ff"))
        button_paths.append(f'<circle cx="520" cy="{y}" r="11" fill="none" stroke="#c792ea" stroke-width="3"/>')
        button_paths.append(f'<line x1="531" y1="{y}" x2="600" y2="{y}" stroke="#9aa7b0" stroke-width="4"/>')

    labels = []
    for path, label in wires:
        labels.append(path)
        labels.append(svg_text(286, 78 + len(labels) * 16, label, 13, fill="#d7e7ef"))

    content = "\n".join([
        *box(40, 60, 210, 600, "ESP32-C3", [
            "GPIO3  SDA",
            "GPIO4  SCL",
            "GPIO20 RST Si4703",
            "GPIO5  Botao 1",
            "GPIO6  Botao 2",
            "GPIO7  Botao 3",
            "GPIO8  Botao 4",
            "3V3",
            "GND",
            "USB nativo: Serial",
        ]),
        *box(410, 55, 250, 165, "OLED SSD1306", ["VCC -> 3V3", "GND -> GND", "SDA -> GPIO3", "SCL -> GPIO4", "Addr 0x3C"]),
        *box(410, 265, 250, 190, "Si4703", ["VCC -> 3V3", "GND -> GND", "SDIO/SDA -> GPIO3", "SCLK/SCL -> GPIO4", "RST -> GPIO20", "Audio -> jack/amplificador"]),
        *labels,
        *button_paths,
        svg_text(710, 90, "Notas", 18, fill="#ffffff", weight="700"),
        svg_text(710, 124, "I2C partilhado: OLED e Si4703 no mesmo SDA/SCL.", 14, fill="#d7e7ef"),
        svg_text(710, 154, "Botoes ligam ao GND; o firmware usa INPUT_PULLUP.", 14, fill="#d7e7ef"),
        svg_text(710, 184, "GPIO20 e UART0 RX: usar USB CDC no platformio.ini.", 14, fill="#d7e7ef"),
        svg_text(710, 214, "Confirmar pull-ups I2C se os modulos nao os incluirem.", 14, fill="#d7e7ef"),
    ])
    write(DOCS / "wiring.svg", f"""<svg xmlns="http://www.w3.org/2000/svg" width="1040" height="720" viewBox="0 0 1040 720">
  <title>Esquema de ligacoes ESP32-C3 FM Radio</title>
  <rect width="1040" height="720" fill="#071019"/>
  {content}
</svg>
""")


def make_index():
    cards = []
    names = [
        ("splash.svg", "Apresentacao"),
        ("01-principal-rds.svg", "Principal com RDS"),
        ("01-principal-sem-rds.svg", "Principal sem RDS"),
        ("02-sintonia.svg", "Sintonia"),
        ("03-volume.svg", "Volume"),
        ("04-presets.svg", "Presets"),
        ("05-scan-auto.svg", "Scan AUTO"),
        ("06-menu.svg", "Menu"),
        ("07-mensagem.svg", "Mensagem"),
        ("08-protecao-ecra.svg", "Protecao de ecra"),
    ]
    for i, (file, label) in enumerate(names):
        x = 20 + (i % 2) * 560
        y = 50 + (i // 2) * 350
        svg_data = (SCREENS / file).read_bytes()
        encoded = b64encode(svg_data).decode("ascii")
        cards.append(svg_text(x, y - 14, label, 18, fill="#ffffff", weight="700"))
        cards.append(f'<image href="data:image/svg+xml;base64,{encoded}" x="{x}" y="{y}" width="512" height="256"/>')
    write(DOCS / "screens-overview.svg", f"""<svg xmlns="http://www.w3.org/2000/svg" width="1100" height="1810" viewBox="0 0 1100 1810">
  <title>Exemplos dos ecras OLED</title>
  <rect width="1100" height="1810" fill="#071019"/>
  {svg_text(20, 30, "ESP32-C3 FM Radio - exemplos dos ecras OLED", 22, fill="#ffffff", weight="700")}
  {"".join(cards)}
</svg>
""")


if __name__ == "__main__":
    make_screens()
    make_wiring()
    make_index()
    print(f"Generated assets in {DOCS}")
