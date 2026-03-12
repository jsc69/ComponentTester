import sys
import re
import math

# =============================================================================
# SYMBOL DIRECTORY
# =============================================================================
#
# Schlüssel: KiCad-Bibliotheksname  (z.B. "Device:R")
# Unter-Schlüssel: KiCad-Rotationswinkel als Integer  0 | 90 | 180 | 270
#
# Wert pro Winkel: entweder ein Alias-Integer  →  "gleiche Matrix wie <n>°"
#                 oder ein Dict mit folgenden Feldern:
#
#   template  Liste[str]  ASCII-Zeilen der Symbol-Matrix.
#             Platzhalter:
#               {v}  Bauteilwert       (z.B. "470", "5k6", "LED")
#               {l}  Label/Bezeichner  (z.B. "R2", "C1")
#             Leerzeilen und Einrückung sind bedeutsam.
#
#   cx        int   Anker-Spalte innerhalb der Matrix (0-basiert).
#                   Gibt an, auf welcher Spalte der Pin-Draht andockt.
#                   Bei horizontalen Symbolen: linker Pinpunkt.
#                   Bei vertikalen Symbolen:   Mittelachse.
#
#   cy        int   Anker-Zeile innerhalb der Matrix (0-basiert).
#                   Bei horizontalen Symbolen: Zeile auf der Draht verläuft.
#                   Bei vertikalen Symbolen:   oberer Pinpunkt.
#
#   pin_axis  "h"   Pins links + rechts  (Bauteil liegt quer im Schaltplan)
#             "v"   Pins oben + unten    (Bauteil steht aufrecht)
#
#   scale_h   bool  True  →  Matrix wird horizontal auf den Pixel-Abstand
#                            zwischen linkem und rechtem Pin gestreckt.
#                            Die längste Zeile wird gestreckt; kürzere Zeilen
#                            werden mit fill_h-Zeichen zentriert aufgefüllt.
#
#   scale_v   bool  True  →  Matrix wird vertikal auf den Pixel-Abstand
#                            zwischen oberem und unterem Pin gestreckt.
#                            Füllzeilen werden abwechselnd oben und unten
#                            mit fill_v-Zeichen eingefügt.
#
#   fill_h    str   Füllzeichen beim horizontalen Strecken  (default "-")
#   fill_v    str   Füllzeichen beim vertikalen  Strecken   (default "|")
#
# Aliase:
#   Ein Winkel-Wert kann ein Integer sein statt eines Dict.
#   Bedeutung: "nimm dieselbe Matrix wie für <n> Grad".
#   Beispiel:  180: 0   →  bei 180° identisch mit 0°
#
# Neue Bauteile eintragen:
#   1. Neuen Eintrag unter dem KiCad-Bibliotheksschlüssel anlegen.
#   2. Alle benötigten Winkel definieren (mindestens 0).
#   3. Für symmetrische Lagen Alias-Integers eintragen.
#   4. cx/cy passend zur Pin-Position setzen.
#
# =============================================================================

SYMBOL_DIR = {

    # ── Widerstand ────────────────────────────────────────────────────────────
    # KiCad: rot=0/180 → vertikal (Pins oben+unten)
    #        rot=90/270 → horizontal (Pins links+rechts)
    "Device:R": {
        0: {
            "template": [
                "  |  ",
                "[{v}]",
                "  |  ",
            ],
            "cx": 2, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,   # identisch mit 0°
        90: {
            "template": [
                "---[{v}]---",
            ],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: 90,  # identisch mit 90°
    },

    # ── Kondensator ───────────────────────────────────────────────────────────
    # KiCad: rot=0/180 → vertikal, rot=90/270 → horizontal
    "Device:C": {
        0: {
            "template": [
                " | ",
                "===",
                "---",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,   # identisch mit 0°
        90: {
            "template": [
                "--| |--",
            ],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: 90,  # identisch mit 90°
    },

    # ── LED ───────────────────────────────────────────────────────────────────
    # KiCad Pins: Pin1=K (Kathode), Pin2=A (Anode)
    # rot=0:   Pin1(K) oben,   Pin2(A) unten  → Pfeil zeigt nach unten (V)
    # rot=180: Pin1(K) unten,  Pin2(A) oben   → Pfeil zeigt nach oben  (^)
    # rot=90:  Pin1(K) rechts, Pin2(A) links  → Pfeil zeigt nach links (<)  ← A links, K rechts
    # rot=270: Pin1(K) links,  Pin2(A) rechts → Pfeil zeigt nach rechts (>) ← A rechts, K links
    "Device:LED": {
        0: {
            "template": [
                " | ",
                "/V\\",
                "-+-",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: {
            "template": [
                " | ",
                "-+-",
                "\\^/",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        90: {
            "template": [
                "--|<|--",
            ],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: {
            "template": [
                "--|>|--",
            ],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
    },

    # ── Diode ─────────────────────────────────────────────────────────────────
    # Gleiche Pin-Logik wie LED (K=Pin1, A=Pin2)
    "Device:D": {
        0: {
            "template": [
                " | ",
                "/V\\",
                "-+-",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: {
            "template": [
                " | ",
                "-+-",
                "\\^/",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        90: {
            "template": ["--|<|--"],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: {
            "template": ["--|>|--"],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
    },

    # ── Zener-Diode ───────────────────────────────────────────────────────────
    # Gleiche Orientierungen wie Device:D, Symbol-Körper = Z
    "Device:D_Zener": {
        0: {
            "template": [
                " | ",
                "/Z\\",
                "-+-",
                " | ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,   # identisch mit 0° (Zener-Symbol ist symmetrisch)
        90: {
            "template": ["--|Z|--"],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: 90,  # identisch mit 90°
    },

    # ── Spule / Induktivität ─────────────────────────────────────────────────
    "Device:L": {
        0: {
            "template": [
                "  |  ",
                " (o) ",
                " (o) ",
                " (o) ",
                "  |  ",
            ],
            "cx": 2, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,   # identisch mit 0°
        90: {
            "template": ["--uuu--"],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: 90,  # identisch mit 90°
    },

    # ── Schalter ──────────────────────────────────────────────────────────────
    "Device:SW": {
        0: {
            "template": [
                "  |  ",
                "  /  ",
                "  |  ",
            ],
            "cx": 2, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,   # identisch mit 0°
        90: {
            "template": ["--/ --"],
            "cx": 0, "cy": 0,
            "pin_axis": "h",
            "scale_h": True,
        },
        270: 90,  # identisch mit 90°
    },

    # ── Transistor NPN ────────────────────────────────────────────────────────
    # Vereinfachte Darstellung; 3 Pins → nur Basis-Verbindung gezeigt
    "Device:Q_NPN_BCE": {
        0: {
            "template": [
                " |  ",
                "-+> ",
                " |  ",
            ],
            "cx": 1, "cy": 0,
            "pin_axis": "v",
            "scale_v": True,
            "fill_v": "|",
        },
        180: 0,
        90:  0,
        270: 0,
    },

    # ── Versorgungsspannungen (Power-Symbole) ─────────────────────────────────
    # Power-Symbole haben nur 1 Pin und werden nicht gestreckt.
    # cy zeigt auf die Pin-Zeile (Anker-Zeile).
    "power:+5V": {
        0: {
            "template": [
                "+5V",
                " /\\ ",
                "  | ",
            ],
            "cx": 2, "cy": 2,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,  # identisch mit 0°
        180: 0,
        270: 0,
    },
    "power:+3V3": {
        0: {
            "template": [
                "+3V3",
                "  /\\",
                "   |",
            ],
            "cx": 3, "cy": 2,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
    "power:+12V": {
        0: {
            "template": [
                "+12V",
                "  /\\",
                "   |",
            ],
            "cx": 3, "cy": 2,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
    "power:VCC": {
        0: {
            "template": [
                "VCC",
                " /\\ ",
                "  | ",
            ],
            "cx": 2, "cy": 2,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
    "power:GND": {
        0: {
            "template": [
                " _|_ ",
                " --- ",
                " GND ",
            ],
            "cx": 2, "cy": 0,   # Anker = oberste Zeile (Pin oben)
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
    "power:GNDD": {
        0: {
            "template": [
                " _|_ ",
                " --- ",
                "GNDD ",
            ],
            "cx": 2, "cy": 0,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
    "power:PWR_FLAG": {
        0: {
            "template": [" PWR "],
            "cx": 2, "cy": 0,
            "pin_axis": "v",
            "scale_v": False,
        },
        90:  0,
        180: 0,
        270: 0,
    },
}

# ── Generische Fallbacks für Bauteile die nicht im SYMBOL_DIR stehen ─────────
_FALLBACK_H = {
    "template": ["---[{v}]---"],
    "cx": 0, "cy": 0,
    "pin_axis": "h",
    "scale_h": True,
}
_FALLBACK_V = {
    "template": [
        "  |  ",
        "[{v}]",
        "  |  ",
    ],
    "cx": 2, "cy": 0,
    "pin_axis": "v",
    "scale_v": True,
    "fill_v": "|",
}


def get_sym_entry(lib, rot, pin_axis_hint="h"):
    """
    Gibt den Symbol-Dict-Eintrag für lib + KiCad-Rotation zurück.
    rot:           KiCad-Winkel (0, 90, 180, 270)
    pin_axis_hint: "h" oder "v" – wird für Fallback verwendet wenn lib unbekannt.

    Alias-Auflösung:  Ist der Wert ein Integer statt Dict, wird dieser Winkel
                      als Verweis auf einen anderen Winkel-Eintrag interpretiert.
    """
    sym = SYMBOL_DIR.get(lib)
    if sym is None:
        return _FALLBACK_H if pin_axis_hint == "h" else _FALLBACK_V

    entry = sym.get(rot, sym.get(0))          # Winkel suchen, Fallback auf 0°
    # Alias auflösen (max. 1 Stufe)
    if isinstance(entry, int):
        entry = sym.get(entry, sym.get(0))
    if entry is None:
        return _FALLBACK_H if pin_axis_hint == "h" else _FALLBACK_V
    return entry


# =============================================================================
# Symbol-Matrix skalieren & Platzhalter ersetzen
# =============================================================================

def _fill_placeholders(line, val, label):
    return line.replace("{v}", val).replace("{l}", label)


def _scale_h(template, target_w, val, label):
    """Streckt die längste Zeile auf target_w Pixel; kürzere Zeilen werden zentriert."""
    lines = [_fill_placeholders(l, val, label) for l in template]
    max_w = max(len(l) for l in lines)
    if max_w >= target_w:
        return lines

    extra = target_w - max_w
    result = []
    for line in lines:
        if len(line) == max_w:
            add_l = extra // 2
            add_r = extra - add_l
            # Streckt die längste Zeile: linkes und rechtes Ende bleibt, Mitte wird gedehnt
            result.append(line[0] + "-" * add_l + line[1:-1] + "-" * add_r + line[-1])
        else:
            diff = target_w - len(line)
            result.append(" " * (diff // 2) + line + " " * (diff - diff // 2))
    return result


def _scale_v(template, target_h, val, label, fill_char="|"):
    """Streckt die Matrix vertikal auf target_h durch Einfügen von Füllzeilen."""
    lines = [_fill_placeholders(l, val, label) for l in template]
    w = max(len(l) for l in lines)
    cx = w // 2
    fill_line = " " * cx + fill_char + " " * (w - cx - 1)

    while len(lines) < target_h:
        lines.insert(0, fill_line)
        if len(lines) < target_h:
            lines.append(fill_line)
    return lines[:target_h] if len(lines) > target_h else lines


def build_sym_lines(entry, val, label, target_w=None, target_h=None):
    """
    Rendert ein Symbol-Template in eine Liste von Zeilen.
    val:      Bauteilwert  (ersetzt {v})
    label:    Bezeichner   (ersetzt {l})
    target_w: Ziel-Pixelbreite  (für scale_h)
    target_h: Ziel-Pixelhöhe    (für scale_v)
    """
    tmpl   = entry["template"]
    fill_v = entry.get("fill_v", "|")

    if entry.get("scale_h") and target_w is not None:
        return _scale_h(tmpl, target_w, val, label)
    if entry.get("scale_v") and target_h is not None:
        return _scale_v(tmpl, target_h, val, label, fill_v)
    return [_fill_placeholders(l, val, label) for l in tmpl]


# =============================================================================
# Parser
# =============================================================================

def parse_kicad_sch(content):
    tokens = re.findall(r'\(|\)|"[^"]*"|[^\s()]+', re.sub(r';[^\n]*', '', content))
    pos = [0]

    def parse_node():
        if pos[0] >= len(tokens):
            return None
        tok = tokens[pos[0]]
        if tok == '(':
            pos[0] += 1
            children = []
            while pos[0] < len(tokens) and tokens[pos[0]] != ')':
                node = parse_node()
                if node is not None:
                    children.append(node)
            if pos[0] < len(tokens):
                pos[0] += 1
            return children
        else:
            pos[0] += 1
            return tok.strip('"')

    results = []
    while pos[0] < len(tokens):
        node = parse_node()
        if node is not None:
            results.append(node)
    return results[0] if results else []


def get_tag(item):
    return str(item[0]).lower() if isinstance(item, list) and item else ""


# =============================================================================
# lib_symbols: Pin-Positionen extrahieren
# =============================================================================

def extract_lib_pins(kicad_sch_node):
    lib_pins = {}
    ls_node = None
    for child in kicad_sch_node:
        if isinstance(child, list) and get_tag(child) == 'lib_symbols':
            ls_node = child
            break
    if ls_node is None:
        return lib_pins

    for sym in ls_node[1:]:
        if not isinstance(sym, list) or get_tag(sym) != 'symbol':
            continue
        sym_name = sym[1] if len(sym) > 1 else ""
        pins = []

        def collect_pins(node):
            if not isinstance(node, list):
                return
            if get_tag(node) == 'pin':
                for c in node:
                    if isinstance(c, list) and get_tag(c) == 'at' and len(c) >= 3:
                        pins.append((float(c[1]), float(c[2])))
                        break
            for c in node:
                collect_pins(c)

        collect_pins(sym)
        if pins:
            lib_pins[sym_name] = pins

    return lib_pins


# =============================================================================
# Pin-Rotation
# =============================================================================

def effective_rot(p1x, p1y, p2x, p2y):
    """
    Berechnet den effektiven Rotationswinkel (0/90/180/270) aus den
    Welt-Koordinaten der beiden Pins.
    Dieser Winkel entspricht der KiCad-Konvention und wird als Schlüssel
    in SYMBOL_DIR verwendet:
      90  → horizontal, Pin1 links  (Strom/Pfeil nach rechts)
      270 → horizontal, Pin1 rechts (Strom/Pfeil nach links)
      0   → vertikal,   Pin1 oben   (Strom/Pfeil nach unten)
      180 → vertikal,   Pin1 unten  (Strom/Pfeil nach oben)
    """
    dx = p2x - p1x
    dy = p2y - p1y
    if abs(dx) >= abs(dy):
        return 90 if dx >= 0 else 270
    else:
        return 0 if dy >= 0 else 180


def rotate_pt(x, y, deg):
    """Rotiert einen Punkt (x,y) um deg Grad um den Ursprung (KiCad-Koordinatensystem)."""
    rad = math.radians(deg)
    rx =  x * math.cos(rad) + y * math.sin(rad)
    ry = -x * math.sin(rad) + y * math.cos(rad)
    return round(rx, 2), round(ry, 2)
    rad = math.radians(deg)
    rx =  x * math.cos(rad) + y * math.sin(rad)
    ry = -x * math.sin(rad) + y * math.cos(rad)
    return round(rx, 2), round(ry, 2)


# =============================================================================
# Elemente extrahieren
# =============================================================================

def extract_elements(tree, lib_pins):
    symbols, wires, junctions = [], [], []

    def walk(data):
        if not isinstance(data, list):
            return
        tag = get_tag(data)

        if tag == 'symbol' and any(isinstance(s, list) and get_tag(s) == 'lib_id' for s in data):
            lib, val, ref, pos_xy, rot = "", "", "", [0.0, 0.0], 0
            for sub in data:
                if not isinstance(sub, list):
                    continue
                t = get_tag(sub)
                if t == 'lib_id':
                    lib = str(sub[1])
                elif t == 'at':
                    pos_xy = [float(sub[1]), float(sub[2])]
                    rot = int(float(sub[3])) if len(sub) > 3 else 0
                elif t == 'property' and len(sub) > 2:
                    if sub[1] == 'Value':
                        val = str(sub[2])
                    elif sub[1] == 'Reference':
                        ref = str(sub[2])
            if lib:
                lib_key = ":".join(lib.split(":")[:2])
                raw_pins = lib_pins.get(lib_key, [])
                world_pins = []
                for px, py in raw_pins:
                    rx, ry = rotate_pt(px, py, rot)
                    wx = round(pos_xy[0] + rx, 2)
                    wy = round(pos_xy[1] + ry, 2)
                    world_pins.append((wx, wy))
                symbols.append({
                    'lib': lib_key, 'val': val, 'ref': ref,
                    'x': pos_xy[0], 'y': pos_xy[1], 'rot': rot,
                    'pins': world_pins,
                })

        elif tag == 'wire':
            pts = []
            for sub in data:
                if isinstance(sub, list) and get_tag(sub) == 'pts':
                    for pt in sub[1:]:
                        if isinstance(pt, list) and get_tag(pt) == 'xy':
                            pts.append((float(pt[1]), float(pt[2])))
            if len(pts) >= 2:
                wires.append({'x1': pts[0][0], 'y1': pts[0][1],
                              'x2': pts[1][0], 'y2': pts[1][1]})

        elif tag == 'junction':
            for sub in data:
                if isinstance(sub, list) and get_tag(sub) == 'at':
                    junctions.append({'x': float(sub[1]), 'y': float(sub[2])})

        for child in data:
            if isinstance(child, list):
                walk(child)

    walk(tree)
    return symbols, wires, junctions


# =============================================================================
# Minimale Symbol-Dimensionen (für Constraint-Berechnung)
# =============================================================================

def min_sym_w(s):
    """Mindest-Pixelbreite für horizontale Darstellung (erot=90 oder 270)."""
    if len(s['pins']) >= 2:
        p1x, p1y = s['pins'][0]
        p2x, p2y = s['pins'][1]
        erot = effective_rot(p1x, p1y, p2x, p2y)
    else:
        erot = 90
    entry = get_sym_entry(s['lib'], erot, pin_axis_hint="h")
    label = s.get('ref', '') if not s.get('ref', '').startswith('#') else ''
    lines = [_fill_placeholders(l, s['val'], label) for l in entry["template"]]
    return max(len(l) for l in lines)


def min_sym_h(s):
    """Mindest-Pixelhöhe für vertikale Darstellung (erot=0 oder 180)."""
    if len(s['pins']) >= 2:
        p1x, p1y = s['pins'][0]
        p2x, p2y = s['pins'][1]
        erot = effective_rot(p1x, p1y, p2x, p2y)
    else:
        erot = 0
    entry = get_sym_entry(s['lib'], erot, pin_axis_hint="v")
    label = s.get('ref', '') if not s.get('ref', '').startswith('#') else ''
    lines = [_fill_placeholders(l, s['val'], label) for l in entry["template"]]
    return len(lines)


# =============================================================================
# Constraint-basiertes Koordinaten-Mapping
# =============================================================================

def build_pixel_map(all_k, symbols, horizontal):
    n = len(all_k)
    default_gap = 3 if horizontal else 2
    min_gap = [default_gap] * max(n - 1, 1)

    ky_with_h_sym = set()

    for s in symbols:
        if len(s['pins']) < 2:
            continue
        p1x, p1y = s['pins'][0]
        p2x, p2y = s['pins'][1]

        if horizontal:
            is_h = abs(p1y - p2y) < 0.1
            if not is_h:
                continue
            lk, rk = min(p1x, p2x), max(p1x, p2x)
            needed = min_sym_w(s) - 1
        else:
            is_v = abs(p1x - p2x) < 0.1
            if not is_v:
                continue
            lk, rk = min(p1y, p2y), max(p1y, p2y)
            needed = min_sym_h(s) - 1
            if not s.get('ref', '').startswith('#') and abs(p1y - p2y) < 0.1:
                ky_with_h_sym.add(s['y'])

        gaps = [i for i in range(n - 1)
                if all_k[i] >= lk - 0.01 and all_k[i + 1] <= rk + 0.01]
        if not gaps:
            continue
        current = sum(min_gap[i] for i in gaps)
        if current < needed:
            extra = needed - current
            per = (extra + len(gaps) - 1) // len(gaps)
            for i in gaps:
                min_gap[i] += per

    # Für Y-Achse: Extra-Zeile vor horizontalen Bauteil-Reihen (für Labels)
    if not horizontal:
        ky_with_h_sym = set()
        for s in symbols:
            if len(s['pins']) < 2 or s.get('ref', '').startswith('#'):
                continue
            p1x, p1y = s['pins'][0]
            p2x, p2y = s['pins'][1]
            if abs(p1y - p2y) < 0.1:
                ky_with_h_sym.add(s['y'])
        for i in range(n - 1):
            if all_k[i + 1] in ky_with_h_sym:
                min_gap[i] = max(min_gap[i], 3)

    px = {}
    if not horizontal and all_k and all_k[0] in ky_with_h_sym:
        cur = 5
    else:
        cur = 3
    for i, k in enumerate(all_k):
        px[k] = cur
        if i < n - 1:
            cur += min_gap[i]
    return px


# =============================================================================
# Symbol rendern
# =============================================================================

def render_symbol(s, px_anchor, py_anchor):
    lib   = s['lib']
    val   = s['val']
    ref   = s.get('ref', '')
    label = ref if (ref and not ref.startswith('#')) else ''
    pins  = s['pins']

    def anc(kx, ky):
        bx = min(px_anchor, key=lambda k: abs(k - kx))
        by = min(py_anchor, key=lambda k: abs(k - ky))
        return px_anchor[bx], py_anchor[by]

    bpx, bpy = anc(s['x'], s['y'])

    # ── Power-Symbole (1 Pin) ────────────────────────────────────────────────
    if len(pins) <= 1:
        entry = get_sym_entry(lib, 0, pin_axis_hint="v")
        lines = build_sym_lines(entry, val, label)
        cx = min(entry['cx'], max(len(l) for l in lines) - 1)
        cy = entry['cy']
        return bpx - cx, bpy - cy, lines

    # ── Zweipol-Bauteile ─────────────────────────────────────────────────────
    p1x, p1y = pins[0]
    p2x, p2y = pins[1]
    pp1x, pp1y = anc(p1x, p1y)
    pp2x, pp2y = anc(p2x, p2y)

    # Effektive Rotation aus den tatsächlichen Pin-Weltkoordinaten
    erot  = effective_rot(p1x, p1y, p2x, p2y)
    is_h  = erot in (90, 270)

    if is_h:
        lx, rx = min(pp1x, pp2x), max(pp1x, pp2x)
        sym_w  = rx - lx + 1
        entry  = get_sym_entry(lib, erot, pin_axis_hint="h")
        sym_lines = build_sym_lines(entry, val, label, target_w=sym_w)

        if label:
            label_row = label.center(max(len(l) for l in sym_lines))
            return lx, pp1y - 1, [label_row] + sym_lines
        return lx, pp1y, sym_lines

    else:
        ty, by2 = min(pp1y, pp2y), max(pp1y, pp2y)
        sym_h   = by2 - ty + 1
        entry   = get_sym_entry(lib, erot, pin_axis_hint="v")
        lines   = build_sym_lines(entry, val, label, target_h=sym_h)
        cx      = min(entry.get('cx', 0), max(len(l) for l in lines) - 1)

        if label:
            fill_ch  = entry.get('fill_v', '|')
            best_row = len(lines) // 2
            for i, l in enumerate(lines):
                if l.strip() and l.strip() != fill_ch:
                    best_row = i
            lines = list(lines)
            lines[best_row] = lines[best_row].rstrip() + '  ' + label

        return bpx - cx, ty, lines


# =============================================================================
# Grid
# =============================================================================

class Grid:
    def __init__(self, w, h):
        self.w = w
        self.h = h
        self.cells = [[' '] * w for _ in range(h)]

    def put(self, x, y, ch):
        if 0 <= y < self.h and 0 <= x < self.w:
            cur = self.cells[y][x]
            if cur == '|' and ch == '-': ch = '+'
            elif cur == '-' and ch == '|': ch = '+'
            prio = {'O': 6, '+': 5, '*': 4, '|': 3, '-': 3, '=': 2}
            if prio.get(ch, 1) >= prio.get(cur, 0):
                self.cells[y][x] = ch

    def put_str(self, x, y, s):
        for i, ch in enumerate(s):
            self.put(x + i, y, ch)

    def put_matrix(self, x, y, lines):
        for r, line in enumerate(lines):
            for c, ch in enumerate(line):
                if ch != ' ':
                    self.put(x + c, y + r, ch)

    def hline(self, x1, x2, y):
        for x in range(min(x1, x2), max(x1, x2) + 1):
            self.put(x, y, '-')

    def vline(self, x, y1, y2):
        for y in range(min(y1, y2), max(y1, y2) + 1):
            self.put(x, y, '|')

    def render(self):
        lines = [''.join(row).rstrip() for row in self.cells]
        while lines and not lines[-1].strip():
            lines.pop()
        while lines and not lines[0].strip():
            lines.pop(0)
        return '\n'.join(lines)


# =============================================================================
# Hauptprogramm
# =============================================================================

def main(path):
    with open(path, 'r', encoding='utf-8') as f:
        content = f.read()

    tree     = parse_kicad_sch(content)
    lib_pins = extract_lib_pins(tree)
    symbols, wires, junctions = extract_elements(tree, lib_pins)

    # Alle KiCad-Koordinaten
    all_kx = sorted(set(
        [s['x'] for s in symbols] +
        [px for s in symbols for (px, py) in s['pins']] +
        [w['x1'] for w in wires] + [w['x2'] for w in wires] +
        [j['x'] for j in junctions]
    ))
    all_ky = sorted(set(
        [s['y'] for s in symbols] +
        [py for s in symbols for (px, py) in s['pins']] +
        [w['y1'] for w in wires] + [w['y2'] for w in wires] +
        [j['y'] for j in junctions]
    ))

    px_anchor = build_pixel_map(all_kx, symbols, horizontal=True)
    py_anchor = build_pixel_map(all_ky, symbols, horizontal=False)

    # Symbole rendern
    rendered   = []
    total_w = total_h = 10
    for s in symbols:
        sx, sy, lines = render_symbol(s, px_anchor, py_anchor)
        rendered.append((sx, sy, lines))
        if lines:
            total_w = max(total_w, sx + max(len(l) for l in lines) + 5)
            total_h = max(total_h, sy + len(lines) + 5)

    grid = Grid(total_w, total_h)

    # Drähte
    for w in wires:
        ax1, ay1 = px_anchor[w['x1']], py_anchor[w['y1']]
        ax2, ay2 = px_anchor[w['x2']], py_anchor[w['y2']]
        if ay1 == ay2:
            grid.hline(ax1, ax2, ay1)
        elif ax1 == ax2:
            grid.vline(ax1, ay1, ay2)
        else:
            grid.hline(ax1, ax2, ay1)
            grid.vline(ax2, ay1, ay2)

    # Symbole (über Drähte)
    for sx, sy, lines in rendered:
        grid.put_matrix(sx, sy, lines)

    # Junctions
    for j in junctions:
        jx = px_anchor[min(px_anchor, key=lambda k: abs(k - j['x']))]
        jy = py_anchor[min(py_anchor, key=lambda k: abs(k - j['y']))]
        grid.put(jx, jy, 'O')

    print(grid.render())


main("ComponentTester.kicad_sch")
