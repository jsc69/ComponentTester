# 🔧 ATmega328P Flash-Anleitung & ArduTester Setup

## 1. Check des Anschlusses (Signatur-Prüfung)
Prüfung, ob der Programmer den Chip erkennt. Falls ein ATmega328PB verbaut ist, kann der Zugriff mit `-F` erzwungen werden. `-v` für erweiterte Ausgaben

**Befehl:**
`avrdude -c stk500v2 -P usb -p m328pb -B 10 -v`

**Erwartetes Ergebnis:**
- `Device signature = 0x1e950f` (wird als m328p erkannt)
- `Expected signature for ATmega328PB is 1E 95 16`
- `Fuses OK (E:FF, H:D9, L:62)` (Werkseinstellung)

---

## 2. Fuses setzen (Hardware-Konfiguration)
Dieser Schritt konfiguriert den internen Takt und den Spannungsschutz. Das `-e` löscht den Chip vorab komplett.

**Befehl:**
`avrdude -c stk500v2 -P usb -p m328p -e -U lfuse:w:0xe2:m -U hfuse:w:0xde:m -U efuse:w:0xfd:m`

**Bedeutung der Werte:**
* **lfuse 0xE2:** Interner 8 MHz Oszillator (kein Teiler).
* **hfuse 0xDE:** Bootloader-Größe 512 Bytes, Startadresse 0x3F00.
* **efuse 0xFD:** Brown-out Detection (BOD) auf 2.7V.

---

## 3. Bootloader flashen & Lock-Bits setzen
Hier wird die `optiboot_atmega328.hex` geschrieben und der Schreibschutz für den Bootloader aktiviert (`lock:w:0xef:m`).

**Befehl:**
`avrdude -c stk500v2 -P usb -p m328p -U flash:w:optiboot_atmega328.hex:i -U lock:w:0xef:m`

**Erwarteter Ablauf:**
1. `erasing chip`
2. `writing flash (32768 bytes)`
3. `verifying flash`
4. `writing lock (1 bytes)`
5. `safemode: Fuses OK (E:FD, H:DE, L:E2)`

---

## 4. Arduino IDE Einstellungen (MiniCore)
Für den ArduTester müssen diese Parameter exakt so in der Arduino IDE unter **Werkzeuge** eingestellt werden:

| Option | Wert | Bemerkung |
| :--- | :--- | :--- |
| **Board** | MiniCore -> ATmega328 |
| **Variant** | 328P / 328PA |
| **Clock** | 8 MHz internal |
| **BOD** | 2.7V | Brownout Detection |
| **Compiler LTO** | LTO enabled | Link Time Optimization: Pflicht für ArduTester! |
| **Bootloader** | Yes bootloader |
| **Programmer** | AVRISP mkII |

**Upload:** Nutze den Menüpunkt **Sketch -> Hochladen mit Programmer** (`Strg` + `Umschalt` + `U`).

---

## 5. Troubleshooting (AVRDUDE Fehler)
* **Signature mismatch:** Der Chip meldet sich mit `1E 95 0F` (328P), obwohl du ihn als `328PB` ansprichst. Nutze `-p m328p` im Befehl, um Konsistenz zu wahren.
* **BOD 2.7V:** Falls der Tester bei fast leerer Batterie falsche Werte anzeigt, stellt das BOD-Level (efuse `0xFD`) sicher, dass der Chip rechtzeitig abschaltet.
* **LTO:** Ohne "LTO enabled" wird die Firmware des ArduTesters oft zu groß für den 32KB Speicher des ATmega328P.

---

## 6. Referenz: AVR Device Signaturen
Falls AVRDUDE eine Signatur zurückgibt, kannst du hier prüfen, um welchen Chip es sich handelt:

| Signatur (Hex) | Chip-Modell | Hinweis |
| :--- | :--- | :--- |
| `1E 93 07` | ATmega8 | - |
| `1E 95 0F` | ATmega328P | Der Klassiker |
| `1E 95 14` | ATmega328 | Ohne "P" (PicoPower) |
| `1E 95 16` | ATmega328PB | Neue Version (2x SPI/I2C) |
| `1E 98 01` | ATmega2560 | Arduino Mega |
| `1E 95 87` | ATmega32U4 | Arduino Leonardo/Micro |
| `1E 91 08` | ATtiny85 | 8-Pin Chip |
| `1E 92 06` | ATtiny44 | 14-Pin Chip |
| `1E 93 0B` | ATtiny84 | 14-Pin Chip |
