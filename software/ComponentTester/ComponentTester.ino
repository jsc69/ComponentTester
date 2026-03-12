/*
 * BrutzelBoy_ESP32.ino  —  ArduTester-Anzeige für den BrutzelBoy
 *
 * Empfängt Messdaten vom ArduTester (ATmega328PB) über den
 * GBC-Cartridge-Bus (4-Bit ID auf U1.P0, 8-Bit Data auf U2.P0)
 * und stellt die Ergebnisse auf dem ILI9341 dar.
 *
 * Steuert den ArduTester über den Kommando-Kanal zurück:
 * neuer Test, Signalgenerator, PWM, Selbsttest, Abschalten.
 *
 * Hardware: ESP32-S3-WROOM-N16R8 auf GBCRetrofitBoard V1.3
 * Library:  Brutzelboy (lokal, src/)
 *
 * BUILD-NR bei jedem Flash hochzählen!
 */

#define BUILD_NR  39

// ════════════════════════════════════════════════════════════════
//  HANDSHAKE-PINS  (alle -1 = Polling-Modus, kein Handshake)
//  Sobald WR/RD direkt am ESP32-GPIO verdrahtet sind, hier eintragen.
// ════════════════════════════════════════════════════════════════
#define BB_WR_PIN   -1   // ESP32→ATmega  "Frame bereit"
#define BB_RD_PIN   -1   // ATmega→ESP32  "ACK"
#define BB_CS_PIN   -1   // ATmega→ESP32  "Kommando pending"

// ════════════════════════════════════════════════════════════════
//  INCLUDES
// ════════════════════════════════════════════════════════════════
#include <Brutzelboy.h>   // bringt display.h (SCREEN_WIDTH/HEIGHT), input.h, cartridge.h

// ════════════════════════════════════════════════════════════════
//  BUS-PROTOKOLL IDs
// ════════════════════════════════════════════════════════════════
#define BUS_ID_TYPE       0x00
#define BUS_ID_PINS       0x01
#define BUS_ID_VAL1_LO    0x02
#define BUS_ID_VAL1_HI    0x03
#define BUS_ID_VAL2_LO    0x04
#define BUS_ID_VAL2_HI    0x05
#define BUS_ID_VAL3_LO    0x06
#define BUS_ID_VAL3_HI    0x07
#define BUS_ID_VAL4_LO    0x08
#define BUS_ID_VAL4_HI    0x09
#define BUS_ID_TEXT_MSG   0x0A
#define BUS_ID_UNIT_SCALE 0x0B
#define BUS_ID_BATT_LO    0x0C
#define BUS_ID_BATT_HI    0x0D
#define BUS_ID_SYS_STAT   0x0E
#define BUS_ID_SYNC       0x0F

// TYPE-Codes
#define TYPE_BJT_NPN      0x01
#define TYPE_BJT_PNP      0x11
#define TYPE_N_EMOSFET    0x02
#define TYPE_P_EMOSFET    0x12
#define TYPE_N_DMOSFET    0x03
#define TYPE_P_DMOSFET    0x13
#define TYPE_N_JFET       0x04
#define TYPE_P_JFET       0x14
#define TYPE_IGBT         0x05
#define TYPE_RESISTOR     0x06
#define TYPE_RESISTOR2    0x16
#define TYPE_CAPACITOR    0x07
#define TYPE_INDUCTOR     0x08
#define TYPE_DIODE        0x09
#define TYPE_DIODE2       0x19
#define TYPE_THYRISTOR    0x0A
#define TYPE_TRIAC        0x0B

// TEXT_MSG-Codes
#define MSG_NO_PART      0x01
#define MSG_TESTING      0x02
#define MSG_OVERLOAD     0x03
#define MSG_PROBING      0x04
#define MSG_SHORT        0x05
#define MSG_SHORT_CREATE 0x06   // "Testpins kurzschließen"
#define MSG_SHORT_REMOVE 0x07   // "Kurzschluss entfernen"
#define MSG_RESISTOR     0x08
#define MSG_NONE         0x09

// SYS_STAT-Codes
#define STAT_IDLE    0x00
#define STAT_BUSY    0x01
#define STAT_DONE    0x02
#define STAT_CAL     0x03
#define STAT_CAL_ERR 0xFF

// UNIT_SCALE: Low-Nibble=Prefix, High-Nibble=Dezimalstellen
#define PREFIX_NONE  0
#define PREFIX_PICO  1
#define PREFIX_NANO  2
#define PREFIX_MICRO 3
#define PREFIX_MILLI 4
#define PREFIX_KILO  5
#define PREFIX_MEGA  6

// Kommando-IDs (ESP32 → ATmega)
#define CMD_NOP        0x0
#define CMD_START_TEST  0x1
#define CMD_FREQ_METER  0x2
#define CMD_SIG_GEN     0x3
#define CMD_PWM_GEN     0x4
#define CMD_C_ESR       0x5
#define CMD_SELF_TEST   0x6
#define CMD_POWER_OFF   0x7
#define CMD_CALIBRATE   0x8

// ════════════════════════════════════════════════════════════════
//  DISPLAY-LAYOUT  (Landscape, SCREEN_WIDTH×SCREEN_HEIGHT aus display.h)
//
//   Header  y=0..25   volle Breite (Typ-Name + Batterie)
//   Symbol  x=0..139  linke Hälfte, Mitte cx=70 cy=133
//   ─────── x=140     Trennlinie
//   Werte   x=144..   rechte Spalte
//   Footer             Batterie rechts unten
// ════════════════════════════════════════════════════════════════
#define DISP_W  SCREEN_WIDTH   // 292 aus display.h der Library
#define DISP_H  SCREEN_HEIGHT  // 240
#define HDR_H   32
#define SYM_W   140
#define VAL_X   148   // x-Start Label
#define VAL_V   192   // x-Start Zahlenwert

// Farben RGB565
#define C_BG      0x0000
#define C_VALUE   0xFFFF
#define C_LABEL   0xC618  // Hellgrau
#define C_WARN    0xFFE0  // Gelb
#define C_ERROR   0xF800  // Rot
#define C_NPN     0x07FF  // Cyan
#define C_PNP     0xFD20  // Orange
#define C_FET_N   0x07E0  // Grün
#define C_FET_P   0xF81F  // Magenta
#define C_PASSIVE 0xFFFF  // Weiß
#define C_DKGREY  0x2104

// ════════════════════════════════════════════════════════════════
//  DATENSPEICHER
// ════════════════════════════════════════════════════════════════
struct BusData {
  uint8_t  type      = 0;
  uint8_t  pins      = 0;
  uint16_t val1      = 0,  val2      = 0,  val3      = 0,  val4      = 0;
  uint8_t  val1_unit = 0,  val2_unit = 0,  val3_unit = 0,  val4_unit = 0;
  uint8_t  text_msg  = 0;
  uint16_t batt_mv   = 0;
  uint8_t  sys_stat  = STAT_IDLE;
  uint16_t cal_RiL   = 0;
  uint16_t cal_RiH   = 0;
  uint16_t cal_RZero = 0;
  uint8_t  cal_Cap   = 0;
  int8_t   cal_Ref   = 0;
  int8_t   cal_Comp  = 0;
  bool     cal_ok    = false;
  bool     fresh     = false;
};

BusData  g_bus;
uint32_t g_result_shown_at = 0;
uint8_t  g_pending_unit    = 0;
uint8_t  g_val_write_idx   = 0;
uint8_t  last_id   = 0xFF;
uint8_t  last_data = 0xFF;
uint32_t frame_nr  = 0;
uint16_t last_keys = 0;

// ════════════════════════════════════════════════════════════════
//  BRUTZELBOY-INSTANZ
// ════════════════════════════════════════════════════════════════
Brutzelboy bb(INIT_LCD | INIT_BUTTONS | INIT_CARTRIDGE);

// ════════════════════════════════════════════════════════════════
//  BUS LESEN  (via cartridge_read_reg aus Library)
//  ID: U1.P0 Bits 3:0 direkt (CARD_A0-A3 = ATmega PORTE)
//  Data: U2.P0 direkt (CARD_D0-D7 = ATmega PORTB)
//  Stabilitätsprüfung: beide Register 2× lesen
// ════════════════════════════════════════════════════════════════
bool busRead(uint8_t &id_out, uint8_t &data_out) {
  uint8_t u1a, u1b, u2a, u2b;
  if (cartridge_read_reg(CARTRIDGE_ADDR_U1, AW9523_REG_INPUT_P0, &u1a) != CARTRIDGE_OK ||
      cartridge_read_reg(CARTRIDGE_ADDR_U2, AW9523_REG_INPUT_P0, &u2a) != CARTRIDGE_OK ||
      cartridge_read_reg(CARTRIDGE_ADDR_U1, AW9523_REG_INPUT_P0, &u1b) != CARTRIDGE_OK ||
      cartridge_read_reg(CARTRIDGE_ADDR_U2, AW9523_REG_INPUT_P0, &u2b) != CARTRIDGE_OK) {
    id_out = 0x0F; data_out = 0xFF; return false;
  }
  bool stable = ((u1a & 0x0F) == (u1b & 0x0F)) && (u2a == u2b);

  static uint8_t prev_u1 = 0xFF, prev_u2 = 0xFF;
  if (u1a != prev_u1 || u2a != prev_u2) {
    Serial.printf("[RAW] U1=%02X/%02X U2=%02X/%02X %s\n",
                  u1a, u1b, u2a, u2b, stable ? "OK" : "UNSTABIL");
    prev_u1 = u1a; prev_u2 = u2a;
  }

  if (!stable) { id_out = 0x0F; data_out = 0xFF; return false; }
  id_out   = u1a & 0x0F;
  data_out = u2a;
  return true;
}

// ════════════════════════════════════════════════════════════════
//  KOMMANDO SENDEN  (ESP32 → ATmega über Kommando-Kanal)
//  CS-Flag (U2.P1_3) setzen, Kommando + Parameter schreiben, CS zurück.
//  U2.P1_6 (ATmega 5V) muss dabei HIGH bleiben!
// ════════════════════════════════════════════════════════════════
bool sendCommand(uint8_t cmd, uint8_t param) {
  if (!bb.isCartridgeReady()) return false;

  // CS HIGH (P1_3) + ATmega-Power (P1_6) HIGH
  // Dir P1_3 und P1_6 als Ausgänge sicherstellen
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_DIR_P1,    0xB7); // P1_3+P1_6 = Output
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_OUTPUT_P1, 0x48); // P1_3=1, P1_6=1

  // Kommando-ID auf ID-Bus (bank=0 → U1.P0, 4 Bit)
  // Parameter auf Daten-Bus (bank=1 → U2.P0, 8 Bit)
  int r = cartridge_write(0, cmd & 0x0F);
  if (r == CARTRIDGE_OK) r = cartridge_write(1, param);

  // CS LOW, ATmega-Power bleibt HIGH
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_OUTPUT_P1, 0x40); // P1_6=1, P1_3=0
  // P0 wieder als Input (busRead braucht das)
  cartridge_write_reg(CARTRIDGE_ADDR_U1, AW9523_REG_DIR_P0, 0xFF);
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_DIR_P0, 0xFF);

  Serial.printf("[CMD] 0x%X param=0x%02X %s\n", cmd, param, r == CARTRIDGE_OK ? "OK" : "FEHLER");
  return (r == CARTRIDGE_OK);
}

// ════════════════════════════════════════════════════════════════
//  WERT FORMATIEREN
// ════════════════════════════════════════════════════════════════
char prefixChar(uint8_t p) {
  switch (p) {
    case PREFIX_PICO:  return 'p'; case PREFIX_NANO:  return 'n';
    case PREFIX_MICRO: return 'u'; case PREFIX_MILLI: return 'm';
    case PREFIX_KILO:  return 'k'; case PREFIX_MEGA:  return 'M';
    default: return ' ';
  }
}

String formatValue(uint16_t val, uint8_t unit_byte, const char* unit_str) {
  uint8_t prefix   = unit_byte & 0x0F;
  uint8_t decimals = (unit_byte >> 4) & 0x0F;
  if (decimals > 4) decimals = 0;

  // Auto-Hochskalierung milli/micro wenn >= 1000
  if (prefix == PREFIX_MILLI && val >= 1000) {
    uint32_t i = val/1000, f = val%1000;
    String s = String(i) + ".";
    if (f < 100) s += "0"; if (f < 10) s += "0";
    return s + String(f) + unit_str;
  }
  if (prefix == PREFIX_MICRO && val >= 1000) {
    uint32_t i = val/1000, f = val%1000;
    String s = String(i) + ".";
    if (f < 100) s += "0"; if (f < 10) s += "0";
    return s + String(f) + "m" + unit_str;
  }

  String s;
  if (decimals == 0) {
    s = String(val);
  } else {
    uint32_t div = 1;
    for (uint8_t i = 0; i < decimals; i++) div *= 10;
    uint32_t integer = val / div, frac = val % div;
    s = String(integer) + ".";
    for (uint8_t i = 1; i < decimals; i++) {
      if (frac < (div / 10)) s += "0";
      div /= 10;
    }
    s += String(frac);
  }
  if (prefix != PREFIX_NONE) s += prefixChar(prefix);
  s += unit_str;
  return s;
}

// U2.P1_0 (CARD_CLK/PC3) kurz auf LOW ziehen — wie physischer ArduTester-Button
void pulseClk() {
  // P1_0 als Output LOW (P1_3=CS und P1_6=PWR bleiben Output)
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_OUTPUT_P1, 0x40); // P1_0=0, P1_6=1, P1_3=0
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_DIR_P1,    0x36); // P1_0+P1_3+P1_6 Output
  delay(50);
  // P1_0 wieder loslassen (Input/High-Z)
  cartridge_write_reg(CARTRIDGE_ADDR_U2, AW9523_REG_DIR_P1,    0x37); // P1_0 Input
}

// ════════════════════════════════════════════════════════════════
//  BUTTON-HANDLING  (Flankenauswertung)
// ════════════════════════════════════════════════════════════════
void handleButtons() {
  uint32_t keys    = bb.getInputState();
  uint32_t changed = keys & ~last_keys;
  last_keys = (uint16_t)keys;
  if (!changed) return;

  if (changed & (1 << INPUT_A))      { Serial.println("[BTN] A→START");    pulseClk(); }
  if (changed & (1 << INPUT_B))      { Serial.println("[BTN] B→CALIBRATE"); sendCommand(CMD_CALIBRATE, 0x01); }
  if (changed & (1 << INPUT_SELECT)) { Serial.println("[BTN] SEL→SIG 1k"); sendCommand(CMD_SIG_GEN,   0x06); } // 1kHz
  if (changed & (1 << INPUT_START))  { Serial.println("[BTN] STA→SIGSTP"); sendCommand(CMD_SIG_GEN,   0xFF); } // Stop
  if (changed & (1 << INPUT_MENU))   { Serial.println("[BTN] MENU→PWM50"); sendCommand(CMD_PWM_GEN,   50);   } // 50%
  if (changed & (1 << INPUT_OPTION)) { Serial.println("[BTN] OPT→PWMSTP"); sendCommand(CMD_PWM_GEN,   0x00); } // Stop
}

// ════════════════════════════════════════════════════════════════
//  BUS-FRAME VERARBEITUNG
// ════════════════════════════════════════════════════════════════
void processBusFrame(uint8_t id, uint8_t data) {
  switch (id) {
    case BUS_ID_SYNC:
      if (data == 0xAA) { g_bus = BusData(); g_val_write_idx = 0; g_pending_unit = 0; }
      break;

    case BUS_ID_SYS_STAT:
      if (data > STAT_DONE) break;
      g_bus.sys_stat = data;
      if (data == STAT_DONE) {
        if (g_result_shown_at == 0) {
          g_result_shown_at = millis();
          if (g_bus.sys_stat == STAT_CAL) {
            g_bus.cal_ok = true;
            displayCalibration();
          } else {
            g_bus.fresh = true;
            displayResult();
          }
        }
      } else if (data == STAT_CAL_ERR) {
        if (g_result_shown_at == 0) {
          g_result_shown_at = millis();
          g_bus.cal_ok = false;
          displayCalibration();
        }
      } else if (data == STAT_CAL) {
        g_result_shown_at = 0;
        displayProbing();
      } else if (data == STAT_IDLE) {
        // Ergebnis noch 5s stehen lassen, dann Idle-Screen
        if (g_result_shown_at == 0) {
          displayIdle();
        } else if (millis() - g_result_shown_at > 5000) {
          g_result_shown_at = 0;
          displayIdle();
        }
      } else if (data == STAT_BUSY) {
        g_result_shown_at = 0;
      }
      break;

    case BUS_ID_TEXT_MSG:
      if (g_bus.sys_stat == STAT_CAL) {
        if (data == MSG_SHORT_CREATE) { displayShortCircuit(true);  break; }
        if (data == MSG_SHORT_REMOVE) { displayShortCircuit(false); break; }
        g_bus.cal_Comp = (int8_t)(data - 128);
      } else {
        g_bus.text_msg = data;
        if (data == MSG_PROBING)      { g_result_shown_at = 0; displayProbing(); }
        if (data == MSG_SHORT_CREATE) { displayShortCircuit(true);  }
        if (data == MSG_SHORT_REMOVE) { displayShortCircuit(false); }
      }
      break;
    case BUS_ID_TYPE:       g_bus.type = data; g_val_write_idx = 0; break;
    case BUS_ID_PINS:       g_bus.pins = data;  break;

    case BUS_ID_UNIT_SCALE:
      if ((data & 0x0F) <= PREFIX_MEGA) g_pending_unit = data;
      break;

    case BUS_ID_VAL1_LO: g_bus.val1  = data; break;
    case BUS_ID_VAL1_HI:
      g_bus.val1 |= ((uint16_t)data<<8); g_bus.val1_unit = g_pending_unit; g_pending_unit = 0;
      if (g_bus.sys_stat == STAT_CAL) g_bus.cal_RiL = g_bus.val1;
      break;
    case BUS_ID_VAL2_LO: g_bus.val2  = data; break;
    case BUS_ID_VAL2_HI:
      g_bus.val2 |= ((uint16_t)data<<8); g_bus.val2_unit = g_pending_unit; g_pending_unit = 0;
      if (g_bus.sys_stat == STAT_CAL) g_bus.cal_RiH = g_bus.val2;
      break;
    case BUS_ID_VAL3_LO: g_bus.val3  = data; break;
    case BUS_ID_VAL3_HI:
      g_bus.val3 |= ((uint16_t)data<<8); g_bus.val3_unit = g_pending_unit; g_pending_unit = 0;
      if (g_bus.sys_stat == STAT_CAL) g_bus.cal_RZero = g_bus.val3;
      break;
    case BUS_ID_VAL4_LO: g_bus.val4  = data; break;
    case BUS_ID_VAL4_HI:
      g_bus.val4 |= ((uint16_t)data<<8); g_bus.val4_unit = g_pending_unit; g_pending_unit = 0;
      if (g_bus.sys_stat == STAT_CAL) {
        g_bus.cal_Cap = (uint8_t)(g_bus.val4 & 0xFF);
        g_bus.cal_Ref = (int8_t)(data - 128);
      }
      break;
    case BUS_ID_BATT_LO: g_bus.batt_mv  = data; break;
    case BUS_ID_BATT_HI: g_bus.batt_mv |= ((uint16_t)data<<8); break;
  }
}

// ════════════════════════════════════════════════════════════════
//  DISPLAY-PRIMITIVE
//  µGUI's fillTriangle(x1,y1,x2,y2,h,c) kennt nur achsenparallele
//  Basis-Dreiecke. Für beliebige Dreiecke nutzen wir fillTri3().
// ════════════════════════════════════════════════════════════════

// Beliebiges gefülltes Dreieck über Framebuffer-Pixel
// (Barycentric-Rasterizer — kein Float, nur Integer)
void fillTri3(int16_t ax, int16_t ay,
              int16_t bx, int16_t by,
              int16_t cx, int16_t cy, uint16_t col) {
  // Bounding-Box
  int16_t minx = min(ax, min(bx, cx));
  int16_t maxx = max(ax, max(bx, cx));
  int16_t miny = min(ay, min(by, cy));
  int16_t maxy = max(ay, max(by, cy));
  // Clip
  if (minx < 0) minx = 0; if (maxx >= DISP_W) maxx = DISP_W-1;
  if (miny < 0) miny = 0; if (maxy >= DISP_H) maxy = DISP_H-1;

  uint16_t* fb = bb.getFramebuffer();
  if (!fb) return;

  for (int16_t py = miny; py <= maxy; py++) {
    for (int16_t px = minx; px <= maxx; px++) {
      // Vorzeichen der Kreuzprodukte
      int32_t d0 = (int32_t)(bx-ax)*(py-ay) - (int32_t)(by-ay)*(px-ax);
      int32_t d1 = (int32_t)(cx-bx)*(py-by) - (int32_t)(cy-by)*(px-bx);
      int32_t d2 = (int32_t)(ax-cx)*(py-cy) - (int32_t)(ay-cy)*(px-cx);
      if ((d0>=0 && d1>=0 && d2>=0) || (d0<=0 && d1<=0 && d2<=0))
        fb[py * DISP_W + px] = col;
    }
  }
}

// Kurzformen für bb.draw*
#define SL(x1,y1,x2,y2)  bb.drawLine((x1),(y1),(x2),(y2),sc)
#define SH(x,y,w)         bb.drawLine((x),(y),(x)+(w)-1,(y),sc)
#define SV(x,y,h)         bb.drawLine((x),(y),(x),(y)+(h)-1,sc)
#define SR(x,y,w,h)       bb.drawFrame((x),(y),(x)+(w)-1,(y)+(h)-1,sc)
#define SF(ax,ay,bx,by,cx,cy)  fillTri3((ax),(ay),(bx),(by),(cx),(cy),sc)
#define SC(x,y,r)         bb.drawCircle((x),(y),(r),sc)

// Pin-Label "X(Tn)" neben Symbol-Anschluss
void drawPinLabel(int16_t x, int16_t y, uint8_t tp,
                  const char* role, uint16_t col, bool alignRight = false) {
  char buf[8]; snprintf(buf, sizeof(buf), "%s(T%d)", role, tp);
  bb.setFont(&FONT_6X8); bb.setTextcolor(col);
  bb.drawString(alignRight ? x - (int16_t)(strlen(buf)*6) : x + 2, y - 3, buf);
}

// ════════════════════════════════════════════════════════════════
//  SCHALTSYMBOLE  (Symbol-Mitte: cx=70, cy=HDR_H+(DISP_H-HDR_H)/2≈133)
// ════════════════════════════════════════════════════════════════

void symBJT(int16_t cx, int16_t cy, bool isNPN,
            uint8_t tp_b, uint8_t tp_c, uint8_t tp_e, uint16_t sc) {
  SC(cx, cy, 24);
  SV(cx-24, cy-11, 22);
  SH(cx-38, cy, 14);   SH(cx-24, cy-11, 14);   SH(cx-24, cy+11, 14);
  SL(cx-10, cy-11, cx+16, cy-27);
  SL(cx-10, cy+11, cx+16, cy+27);
  if (isNPN) SF(cx+16,cy+27, cx+8,cy+18, cx+18,cy+19);   // Pfeil Emitter-Ende
  else       SF(cx-10,cy+11, cx-2,cy+20, cx-2,cy+8);     // Pfeil Emitter-Ansatz
  drawPinLabel(cx-38, cy,    tp_b, "B", sc, true);
  drawPinLabel(cx+17, cy-28, tp_c, "C", sc, false);
  drawPinLabel(cx+17, cy+28, tp_e, "E", sc, false);
}

void symMOSFET(int16_t cx, int16_t cy, bool isN, bool isEnh,
               uint8_t tp_g, uint8_t tp_d, uint8_t tp_s, uint16_t sc) {
  SH(cx-36, cy, 16);
  SV(cx-20, cy-18, 36);
  if (isEnh) { SV(cx-13,cy-18,9); SV(cx-13,cy-4,8); SV(cx-13,cy+9,9); }
  else         SV(cx-13, cy-18, 36);
  SH(cx-13, cy-13, 30);  SH(cx-13, cy+13, 30);
  SV(cx+17, cy-24, 11);  SV(cx+17, cy+13, 11);
  if (isN) SF(cx-7,cy, cx-14,cy-4, cx-14,cy+4);   // Pfeil →
  else     SF(cx-14,cy, cx-7,cy-4, cx-7,cy+4);    // Pfeil ←
  SV(cx-14, cy, 13);
  drawPinLabel(cx-36, cy,    tp_g, "G", sc, true);
  drawPinLabel(cx+19, cy-26, tp_d, "D", sc, false);
  drawPinLabel(cx+19, cy+26, tp_s, "S", sc, false);
}

void symJFET(int16_t cx, int16_t cy, bool isN,
             uint8_t tp_g, uint8_t tp_d, uint8_t tp_s, uint16_t sc) {
  SV(cx-5, cy-22, 44);
  SH(cx-22, cy, 17);
  if (isN) SF(cx-5,cy, cx-13,cy-4, cx-13,cy+4);
  else     SF(cx-14,cy, cx-6,cy-4, cx-6,cy+4);
  SH(cx-5, cy-14, 24);  SV(cx+19, cy-24, 10);
  SH(cx-5, cy+14, 24);  SV(cx+19, cy+14, 10);
  drawPinLabel(cx-22, cy,    tp_g, "G", sc, true);
  drawPinLabel(cx+21, cy-26, tp_d, "D", sc, false);
  drawPinLabel(cx+21, cy+26, tp_s, "S", sc, false);
}

void symResistor(int16_t cx, int16_t cy, uint8_t tp1, uint8_t tp2, uint16_t sc) {
  SH(cx-40, cy, 20);  SH(cx+20, cy, 20);
  SR(cx-20, cy-9, 40, 18);
  char buf[4]; bb.setFont(&FONT_6X8); bb.setTextcolor(sc);
  snprintf(buf,4,"T%d",tp1); bb.drawString(cx-42-(int16_t)(strlen(buf)*6), cy-3, buf);
  snprintf(buf,4,"T%d",tp2); bb.drawString(cx+41, cy-3, buf);
}

void symCapacitor(int16_t cx, int16_t cy, uint8_t tp1, uint8_t tp2, uint16_t sc) {
  SH(cx-36, cy, 30);  SH(cx+6, cy, 30);
  SV(cx-6, cy-15, 30);  SV(cx+6, cy-15, 30);
  char buf[4]; bb.setFont(&FONT_6X8); bb.setTextcolor(sc);
  snprintf(buf,4,"T%d",tp1); bb.drawString(cx-38-(int16_t)(strlen(buf)*6), cy-3, buf);
  snprintf(buf,4,"T%d",tp2); bb.drawString(cx+37, cy-3, buf);
}

void symInductor(int16_t cx, int16_t cy, uint8_t tp1, uint8_t tp2, uint16_t sc) {
  SH(cx-40, cy, 16);  SH(cx+24, cy, 16);
  for (int i = 0; i < 4; i++) {
    int16_t bx = cx - 24 + i * 12;
    for (int a = 0; a <= 160; a += 10) {
      float r1 = radians(a) + (float)M_PI, r2 = radians(a+10) + (float)M_PI;
      SL(bx+6+(int16_t)(6*cosf(r1)), cy-(int16_t)(6*sinf(r1)),
         bx+6+(int16_t)(6*cosf(r2)), cy-(int16_t)(6*sinf(r2)));
    }
  }
  char buf[4]; bb.setFont(&FONT_6X8); bb.setTextcolor(sc);
  snprintf(buf,4,"T%d",tp1); bb.drawString(cx-42-(int16_t)(strlen(buf)*6), cy-3, buf);
  snprintf(buf,4,"T%d",tp2); bb.drawString(cx+41, cy-3, buf);
}

void symDiode(int16_t cx, int16_t cy, uint8_t tp_a, uint8_t tp_k, uint16_t sc) {
  SH(cx-40, cy, 28);  SH(cx+12, cy, 28);
  SF(cx-12,cy-13,  cx-12,cy+13,  cx+12,cy);   // Dreieck
  SV(cx+12, cy-13, 26);
  drawPinLabel(cx-40, cy, tp_a, "A", sc, true);
  drawPinLabel(cx+40, cy, tp_k, "K", sc, false);
}

void symThyristor(int16_t cx, int16_t cy,
                  uint8_t tp_a, uint8_t tp_k, uint8_t tp_g, uint16_t sc) {
  SV(cx, cy-40, 26);  SV(cx, cy+14, 26);
  SF(cx-14,cy-14, cx+14,cy-14, cx,cy+14);
  SH(cx-14, cy+14, 28);
  SL(cx+14, cy+14, cx+26, cy+26);
  char buf[8]; bb.setFont(&FONT_6X8); bb.setTextcolor(sc);
  snprintf(buf,8,"A(T%d)",tp_a); bb.drawString(cx-(int16_t)(strlen(buf)*3), cy-52, buf);
  snprintf(buf,8,"K(T%d)",tp_k); bb.drawString(cx-(int16_t)(strlen(buf)*3), cy+44, buf);
  snprintf(buf,8,"G(T%d)",tp_g); bb.drawString(cx+28, cy+22, buf);
}

void symTriac(int16_t cx, int16_t cy,
              uint8_t tp_mt2, uint8_t tp_mt1, uint8_t tp_g, uint16_t sc) {
  SV(cx, cy-40, 26);  SV(cx, cy+14, 26);
  SF(cx-14,cy-14, cx+14,cy-14, cx,cy+14);
  SF(cx-14,cy+14, cx+14,cy+14, cx,cy-14);
  SH(cx-14, cy-14, 28);  SH(cx-14, cy+14, 28);
  SL(cx+14, cy, cx+26, cy+18);
  char buf[10]; bb.setFont(&FONT_6X8); bb.setTextcolor(sc);
  snprintf(buf,10,"MT2(T%d)",tp_mt2); bb.drawString(cx-(int16_t)(strlen(buf)*3), cy-52, buf);
  snprintf(buf,10,"MT1(T%d)",tp_mt1); bb.drawString(cx-(int16_t)(strlen(buf)*3), cy+44, buf);
  snprintf(buf,10,"G(T%d)",tp_g);     bb.drawString(cx+28, cy+14, buf);
}

// ════════════════════════════════════════════════════════════════
//  SCREEN-HILFSFUNKTIONEN
// ════════════════════════════════════════════════════════════════

// Testpunkt-Nummer (1..3) für Pinrolle (1=E/S/A, 2=B/G, 3=C/D/K)
uint8_t tpForRole(uint8_t pins_byte, uint8_t role) {
  for (uint8_t i = 0; i < 3; i++)
    if (((pins_byte >> (i*2)) & 0x03) == role) return i + 1;
  return 1;
}

const char* pinLabel(uint8_t type, uint8_t role) {
  switch (type) {
    case TYPE_BJT_NPN: case TYPE_BJT_PNP:
      return role==1?"E": role==2?"B": "C";
    case TYPE_N_EMOSFET: case TYPE_P_EMOSFET:
    case TYPE_N_DMOSFET: case TYPE_P_DMOSFET:
    case TYPE_N_JFET:    case TYPE_P_JFET: case TYPE_IGBT:
      return role==1?"S": role==2?"G": "D";
    case TYPE_DIODE: case TYPE_DIODE2:
      return role==1?"A": "K";
    case TYPE_THYRISTOR: case TYPE_TRIAC:
      return role==1?"A": role==2?"G": "K";
    default: return role==1?"1": role==2?"2": "3";
  }
}

// Header: Typ-Farbe füllt obere Leiste, Batterie rechtsbündig
void drawHeader(const char* title, uint16_t color) {
  bb.fillScreen(C_BG);
  // Header-Balken zeilenweise füllen (sicherer als fillFrame)
  for (int16_t y = 0; y < HDR_H; y++)
    bb.drawLine(0, y, DISP_W-1, y, color);
  // Titel in Schwarz wenn Farbe hell genug, sonst Weiß
  uint8_t r = (color >> 11) & 0x1F;
  uint8_t g = (color >> 5)  & 0x3F;
  uint8_t b =  color        & 0x1F;
  uint16_t txt_col = (r*2 + g + b*2 > 80) ? 0x0000 : 0xFFFF;
  bb.setFont(&FONT_8X12); bb.setTextcolor(txt_col);
  bb.setBackcolor(color);          // Hintergrund = Header-Farbe → kein schwarzes Rechteck
  bb.drawString(5, 10, title);
  bb.setBackcolor(C_BG);           // zurücksetzen für den Rest
  if (g_bus.batt_mv > 0) {
    char buf[10]; snprintf(buf,sizeof(buf),"%d.%02dV",
                           g_bus.batt_mv/1000, (g_bus.batt_mv%1000)/10);
    bb.setFont(&FONT_6X8); bb.setTextcolor(txt_col); bb.setBackcolor(color);
    bb.drawString(DISP_W-(int16_t)(strlen(buf)*6)-4, 9, buf);
    bb.setBackcolor(C_BG);
  }
  bb.drawLine(SYM_W, HDR_H, SYM_W, DISP_H-1, C_DKGREY);
}

// Zeile rechte Spalte: Label + Wert + Trennlinie
void drawRow(int16_t y, const char* label, const String& value, uint16_t val_col = C_VALUE) {
  bb.setFont(&FONT_8X12);
  bb.setTextcolor(C_LABEL); bb.drawString(VAL_X, y, label);
  bb.setTextcolor(val_col); bb.drawString(VAL_V, y, value.c_str());
  bb.drawLine(VAL_X, y+14, DISP_W-1, y+14, C_DKGREY);
}

// Footer: Batterie rechts unten
void drawFooter() {
  if (!g_bus.batt_mv) return;
  char buf[14]; snprintf(buf,sizeof(buf),"Batt: %d.%02dV",
                         g_bus.batt_mv/1000, (g_bus.batt_mv%1000)/10);
  const int16_t fy = DISP_H - 10;
  bb.drawLine(VAL_X, fy-2, DISP_W-1, fy-2, C_LABEL);
  bb.setFont(&FONT_6X8); bb.setTextcolor(C_LABEL);
  bb.drawString(VAL_X, fy, buf);
}

// ════════════════════════════════════════════════════════════════
//  SCREEN-FUNKTIONEN
// ════════════════════════════════════════════════════════════════

void displayShortCircuit(bool create) {
  bb.fillScreen(C_BG);
  drawHeader(create ? "Kalibrierung" : "Kalibrierung", 0x8410);  // Dunkelgrau
  int16_t y = HDR_H + 20;
  bb.setFont(&FONT_8X12); bb.setTextcolor(C_VALUE);
  if (create) {
    bb.drawString(8, y,      "Testpins");
    bb.drawString(8, y + 20, "kurzschliessen!");
  } else {
    bb.drawString(8, y,      "Kurzschluss");
    bb.drawString(8, y + 20, "entfernen!");
  }
  bb.updateDisplay();
}

void displayCalibration() {
  uint16_t hdr_col = g_bus.cal_ok ? 0x07E0 : C_ERROR;  // Grün / Rot
  drawHeader(g_bus.cal_ok ? "Kalibrierung OK" : "Kalibrierung ERR", hdr_col);

  int16_t y = HDR_H + 6;
  char val[16];

  snprintf(val, sizeof(val), "%u Om", g_bus.cal_RiL);
  drawRow(y, "RiL",  String(val)); y += 18;

  snprintf(val, sizeof(val), "%u Om", g_bus.cal_RiH);
  drawRow(y, "RiH",  String(val)); y += 18;

  snprintf(val, sizeof(val), "%u Om", g_bus.cal_RZero);
  drawRow(y, "R0",   String(val)); y += 18;

  snprintf(val, sizeof(val), "%u pF", g_bus.cal_Cap);
  drawRow(y, "C0",   String(val)); y += 18;

  snprintf(val, sizeof(val), "%+d mV", (int)g_bus.cal_Ref);
  drawRow(y, "Ref",  String(val)); y += 18;

  snprintf(val, sizeof(val), "%+d mV", (int)g_bus.cal_Comp);
  drawRow(y, "Comp", String(val));

  bb.updateDisplay();
}

void displayIdle() {
  bb.fillScreen(C_BG);
  bb.setFont(&FONT_8X12); bb.setTextcolor(C_DKGREY);
  bb.drawString(10, DISP_H/2 - 6, "Bauteil einlegen...");
  bb.updateDisplay();
}
void displayProbing() {
  bb.fillScreen(C_BG);
  bb.setFont(&FONT_8X12); bb.setTextcolor(C_VALUE);
  bb.drawString(10, DISP_H/2 - 6, "Messe...");
  bb.updateDisplay();
}


void displayError(const char* msg) {
  bb.fillScreen(C_BG);
  bb.setFont(&FONT_10X16); bb.setTextcolor(C_ERROR);
  bb.drawString(10, DISP_H/2 - 8, msg);
  bb.updateDisplay();
}

void displayResult() {
  uint8_t t   = g_bus.type;
  uint8_t tp1 = tpForRole(g_bus.pins, 1);
  uint8_t tp2 = tpForRole(g_bus.pins, 2);
  uint8_t tp3 = tpForRole(g_bus.pins, 3);

  if (g_bus.text_msg == MSG_NO_PART) { displayError("Kein Bauteil"); return; }
  if (g_bus.text_msg == MSG_SHORT)   { displayError("Kurzschluss");  return; }

  const char* title = "Unbekannt";
  uint16_t col = C_PASSIVE;
  switch (t) {
    case TYPE_BJT_NPN:   title="NPN BJT";    col=C_NPN;    break;
    case TYPE_BJT_PNP:   title="PNP BJT";    col=C_PNP;    break;
    case TYPE_N_EMOSFET: title="N-E-MOSFET"; col=C_FET_N;  break;
    case TYPE_P_EMOSFET: title="P-E-MOSFET"; col=C_FET_P;  break;
    case TYPE_N_DMOSFET: title="N-D-MOSFET"; col=C_FET_N;  break;
    case TYPE_P_DMOSFET: title="P-D-MOSFET"; col=C_FET_P;  break;
    case TYPE_N_JFET:    title="N-JFET";     col=C_FET_N;  break;
    case TYPE_P_JFET:    title="P-JFET";     col=C_FET_P;  break;
    case TYPE_IGBT:      title="IGBT";       col=C_FET_N;  break;
    case TYPE_RESISTOR:  title="Widerstand"; col=C_PASSIVE; break;
    case TYPE_RESISTOR2: title="2x Wider.";  col=C_PASSIVE; break;
    case TYPE_CAPACITOR: title="Kondensator";col=C_PASSIVE; break;
    case TYPE_INDUCTOR:  title="Induktiv.";  col=C_PASSIVE; break;
    case TYPE_DIODE:     title="Diode";      col=C_WARN;   break;
    case TYPE_DIODE2:    title="Doppeldiode";col=C_WARN;   break;
    case TYPE_THYRISTOR: title="Thyristor";  col=C_WARN;   break;
    case TYPE_TRIAC:     title="Triac";      col=C_WARN;   break;
  }

  drawHeader(title, col);

  // Symbol links (cx=70, cy=133)
  const int16_t cx = SYM_W/2, cy = HDR_H + (DISP_H-HDR_H)/2;
  switch (t) {
    case TYPE_BJT_NPN:   symBJT(cx,cy,true,  tp2,tp3,tp1,col); break;
    case TYPE_BJT_PNP:   symBJT(cx,cy,false, tp2,tp3,tp1,col); break;
    case TYPE_N_EMOSFET: symMOSFET(cx,cy,true, true, tp2,tp3,tp1,col); break;
    case TYPE_P_EMOSFET: symMOSFET(cx,cy,false,true, tp2,tp3,tp1,col); break;
    case TYPE_N_DMOSFET: symMOSFET(cx,cy,true, false,tp2,tp3,tp1,col); break;
    case TYPE_P_DMOSFET: symMOSFET(cx,cy,false,false,tp2,tp3,tp1,col); break;
    case TYPE_N_JFET:    symJFET(cx,cy,true, tp2,tp3,tp1,col); break;
    case TYPE_P_JFET:    symJFET(cx,cy,false,tp2,tp3,tp1,col); break;
    case TYPE_RESISTOR: case TYPE_RESISTOR2: symResistor(cx,cy,tp1,tp2,col); break;
    case TYPE_CAPACITOR: symCapacitor(cx,cy,tp1,tp2,col); break;
    case TYPE_INDUCTOR:  symInductor(cx,cy,tp1,tp2,col);  break;
    case TYPE_DIODE: case TYPE_DIODE2: symDiode(cx,cy,tp1,tp2,col); break;
    case TYPE_THYRISTOR: symThyristor(cx,cy,tp1,tp3,tp2,col); break;
    case TYPE_TRIAC:     symTriac(cx,cy,tp1,tp3,tp2,col);     break;
    default:
      bb.setFont(&FONT_6X8); bb.setTextcolor(C_ERROR);
      bb.drawString(cx-24, cy, "Unbekannt"); break;
  }

  // Typ-Name unter Symbol (zentriert in linker Hälfte)
  bb.setFont(&FONT_8X12); bb.setTextcolor(col);
  int16_t tx = SYM_W/2 - (int16_t)(strlen(title)*4);
  if (tx < 2) tx = 2;
  bb.drawString(tx, DISP_H - 20, title);

  // Werte rechte Spalte
  int16_t ry = HDR_H + 8;
  switch (t) {
    case TYPE_BJT_NPN: case TYPE_BJT_PNP:
      if (g_bus.val1) { drawRow(ry,"hFE:", formatValue(g_bus.val1,g_bus.val1_unit,""),   col); ry+=18; }
      if (g_bus.val2) { drawRow(ry,"Vbe:", formatValue(g_bus.val2,g_bus.val2_unit,"V"),  col); ry+=18; }
      if (g_bus.val3) { drawRow(ry,"Iceo:",formatValue(g_bus.val3,g_bus.val3_unit,"A"),  col); ry+=18; }
      break;
    case TYPE_N_EMOSFET: case TYPE_P_EMOSFET:
    case TYPE_N_DMOSFET: case TYPE_P_DMOSFET:
    case TYPE_N_JFET:    case TYPE_P_JFET: case TYPE_IGBT:
      if (g_bus.val1) { drawRow(ry,"Vth:", formatValue(g_bus.val1,g_bus.val1_unit,"V"),  col); ry+=18; }
      if (g_bus.val2) { drawRow(ry,"Vgs:", formatValue(g_bus.val2,g_bus.val2_unit,"V"),  col); ry+=18; }
      if (g_bus.val3) { drawRow(ry,"Idss:",formatValue(g_bus.val3,g_bus.val3_unit,"A"),  col); ry+=18; }
      break;
    case TYPE_RESISTOR: case TYPE_RESISTOR2:
      if (g_bus.val1) { drawRow(ry,"R1:",  formatValue(g_bus.val1,g_bus.val1_unit,"Ohm"),col); ry+=18; }
      if (t==TYPE_RESISTOR2 && g_bus.val2)
                       { drawRow(ry,"R2:",  formatValue(g_bus.val2,g_bus.val2_unit,"Ohm"),col); ry+=18; }
      break;
    case TYPE_CAPACITOR:
      if (g_bus.val1) { drawRow(ry,"C:",   formatValue(g_bus.val1,g_bus.val1_unit,"F"),  col); ry+=18; }
      if (g_bus.val2) { drawRow(ry,"ESR:", formatValue(g_bus.val2,g_bus.val2_unit,"Ohm"),col); ry+=18; }
      if (g_bus.val3) { drawRow(ry,"Vls:", formatValue(g_bus.val3,g_bus.val3_unit,"%"),  col); ry+=18; }
      break;
    case TYPE_INDUCTOR:
      if (g_bus.val1) { drawRow(ry,"L:",   formatValue(g_bus.val1,g_bus.val1_unit,"H"),  col); ry+=18; }
      if (g_bus.val2) { drawRow(ry,"R:",   formatValue(g_bus.val2,g_bus.val2_unit,"Ohm"),col); ry+=18; }
      break;
    case TYPE_DIODE: case TYPE_DIODE2:
      if (g_bus.val1) { drawRow(ry,"Vf:",  formatValue(g_bus.val1,g_bus.val1_unit,"V"),  col); ry+=18; }
      if (t==TYPE_DIODE2&&g_bus.val2)
                       { drawRow(ry,"Vf2:", formatValue(g_bus.val2,g_bus.val2_unit,"V"),  col); ry+=18; }
      if (g_bus.val3) { drawRow(ry,"C:",   formatValue(g_bus.val3,g_bus.val3_unit,"F"),  col); ry+=18; }
      break;
    case TYPE_THYRISTOR: case TYPE_TRIAC:
      if (g_bus.val1) { drawRow(ry,"Vgt:", formatValue(g_bus.val1,g_bus.val1_unit,"V"),  col); ry+=18; }
      if (g_bus.val2) { drawRow(ry,"Igt:", formatValue(g_bus.val2,g_bus.val2_unit,"A"),  col); ry+=18; }
      break;
    default: break;
  }

  drawFooter();
  bb.updateDisplay();
}

// ════════════════════════════════════════════════════════════════
//  SETUP
// ════════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Serial.printf("\n[BB] BUILD #%d  %s %s\n", BUILD_NR, __DATE__, __TIME__);

  // Handshake-Pins setzen (alle -1 = Polling-Modus)
  cartridge_config_t cart_cfg = CARTRIDGE_DEFAULT_CONFIG(I2C_NUM_0);
  cart_cfg.handshake = { BB_WR_PIN, BB_RD_PIN, BB_CS_PIN };
  bb.setCartridgeConfig(cart_cfg);

  bb.begin();
  bb.setBrightness(200);

  if (!bb.isCartridgeReady()) {
    bb.fillScreen(C_BG);
    bb.setFont(&FONT_8X12); bb.setTextcolor(C_ERROR);
    bb.drawString(8, 40, "Cartridge Fehler!");
    bb.setFont(&FONT_6X8);  bb.setTextcolor(C_LABEL);
    bb.drawString(8, 60, "AW9523B nicht gefunden");
    bb.drawString(8, 74, "I2C-Adressen pruefen");
    bb.updateDisplay();
    Serial.println("[ERR] Cartridge init fehlgeschlagen");
    return;
  }

  // Debug: U1 Register auslesen
  uint8_t v; char dbg[80];
  cartridge_read_reg(CARTRIDGE_ADDR_U1, AW9523_REG_DIR_P0, &v);
  snprintf(dbg,sizeof(dbg),"[U1-DIAG] DIR_P0=%02X",v);
  cartridge_read_reg(CARTRIDGE_ADDR_U1, AW9523_REG_INPUT_P1, &v);
  Serial.printf("%s INP_P1=%02X present=%s\n",
                dbg, v, cartridge_is_present() ? "JA" : "NEIN");

  displayIdle();
  Serial.println("[BB] Bereit");
}

// ════════════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════════════
void loop() {
  bb.loop();
  handleButtons();

  if (!bb.isCartridgeReady()) {
    vTaskDelay(pdMS_TO_TICKS(10));
    return;
  }

  uint8_t id, data;
  if (!busRead(id, data)) {
    vTaskDelay(pdMS_TO_TICKS(1));  // Andere Tasks atmen lassen
    return;
  }
  if (id == 0x0F) { last_id = 0xFF; last_data = 0xFF; return; }
  if (id == last_id && data == last_data) return;

  frame_nr++;
  Serial.printf("[B%d #%lu] ID=%02X DATA=%02X\n", BUILD_NR, frame_nr, id, data);
  processBusFrame(id, data);
  last_id = id; last_data = data;
}
