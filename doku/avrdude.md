# 🛠️ AVRDUDE Cheat Sheet

## 1. Die Befehlsstruktur
`avrdude -c [Programmer] -p [Chip] -P [Port] [Optionen] -U [Speicher]:[Aktion]:[Datei/Wert]:[Format]`

---

## 2. Hardware-Optionen (Flags)
* **`-c` (Programmer):** Hardware-Typ. Für mkII: `stk500v2` oder `avrispmkII`. Für Arduino als ISP: `avrisp`.
* **`-p` (Part):** Chip-Modell. `m328p` (Standard), `m328pb` (neue Variante).
* **`-P` (Port):** Schnittstelle. `usb` (für mkII), `/dev/ttyUSB0` (Linux), `COM3` (Windows).
* **`-B` (Bitclock):** Setzt die ISP-Geschwindigkeit in µs. 
    * *Wichtig:* Bei 1 MHz Werkseinstellung oder internen 8 MHz ist `-B 10` (oder höher) oft nötig, damit der Programmer nicht "zu schnell" für den Chip spricht.
* **`-e` (Erase):** Löscht den gesamten chip. Erforderlich, bevor Lock-Bits aufgehoben werden können.
* **`-v` (Verbose):** Mehr Details. Hilft bei der Fehlersuche und zeigt die aktuellen Fuses an.
* **`-F` (Force):** Ignoriert eine falsche Signatur. **Nur im Notfall nutzen!**
* **`-n` (No-Write):** Liest nur Informationen aus, schreibt nichts. Ideal zum Testen der Verbindung.

---

## 3. Speicher-Operationen ('-U')
**Syntax:** `<Bereich>:<Aktion>:<Datei/Wert>:<Format>`

* **Bereiche:** `flash` (Programm), `eeprom`, `lfuse` (Low), `hfuse` (High), `efuse` (Extended), `lock` (Lock-bits).
* **Aktionen:** `w` (write), `r` (read), `v` (verify).
* **Formate:** `i` (Intel Hex Datei), `m` (Immediate - Direktwert), `h` (Hexadezimal).

---

## 4. Fuse-Konfigurationen für ATmega328P
Fuses bestimmen die grundlegende Hardware-Logik (Takt, Spannungsschutz).

### A. Interner Oszillator (8 MHz)
*Für minimalistische Aufbauten ohne externen Quarz.*
* **lfuse:w:0xe2:m**: 8 MHz intern, keine Taktteilung, 64ms Start-up Zeit.
* **hfuse:w:0xd9:m**: SPI Programmierung erlaubt, EEPROM bleibt beim Erase erhalten.
* **efuse:w:0xfd:m**: Brown-out Detection (BOD) bei 2.7V.

**Befehl:**
`avrdude -c stk500v2 -P usb -p m328p -U lfuse:w:0xe2:m -U hfuse:w:0xd9:m -U efuse:w:0xfd:m`

### B. Externer Quarz (16 MHz)
*Standard Arduino Uno Setup für präzises Timing.*
* **lfuse:w:0xff:m**: Externer Quarz (Low Power), 16 MHz+, 64ms Start-up Zeit.
* **hfuse:w:0xde:m**: Bootloader-Bereich 512 Bytes (für Optiboot).
* **efuse:w:0xfd:m**: Brown-out Detection bei 2.7V.

**Befehl:**
`avrdude -c stk500v2 -P usb -p m328p -U lfuse:w:0xff:m -U hfuse:w:0xde:m -U efuse:w:0xfd:m`

---

## 5. Lock-Bits (Ausleseschutz & Sicherheit)
Sperrt den Zugriff auf den Chip-Inhalt. Zum Entsperren muss der gesamte Chip mit `-e` gelöscht werden.

* **0x3F (Standard):** Keine Sperre. Alles kann gelesen und geschrieben werden.
* **0xFC (Schreibschutz):** Programm kann nicht mehr überschrieben werden (außer per Chip-Erase).
* **0x00 (Kompletter Schutz):** Weder Lesen noch Schreiben über ISP möglich. Schützt vor Reverse-Engineering.
**Befehl:**
`avrdude -c stk500v2 -P usb -p m328p -U lock:w:0x00:m`

---

## 6. Power-User Befehle (Praxis-Beispiele)

| Aufgabe | Befehl |
| :--- | :--- |
| **Verbindungstest** | avrdude -c stk500v2 -P usb -p m328p -v |
| **Programm flashen** | avrdude -c stk500v2 -P usb -p m328p -U flash:w:dateiname.hex:i |
| **Komplett-Erase** | avrdude -c stk500v2 -P usb -p m328p -e |
| **Rettung (Langsamer Takt)** | avrdude -c stk500v2 -P usb -p m328p -B 250 -v |
| **EEPROM auslesen** | avrdude -c stk500v2 -P usb -p m328p -U eeprom:r:backup.eep:i |

---

## 7. Troubleshooting
* **`Device signature = 0x000000`**: Verkabelung oder Stromversorgung prüfen. Ist die ISP-Geschwindigkeit zu hoch? Probiere `-B 10`.
* **`Verification error`**: Oft ein Zeichen für eine instabile Spannungsquelle oder zu lange/ungeschirmte ISP-Kabel.
* **`ser_open(): can't open device`**: Der Port wird bereits von einem anderen Programm (z.B. Serieller Monitor der Arduino IDE) genutzt.

---

## 8. Brown-out Detection (BOD) Level (efuse)
BOD resettet den Chip, wenn die Versorgungsspannung unter einen kritischen Wert fällt, um Fehlberechnungen oder EEPROM-Fehler zu vermeiden.

* **0xFF**: BOD deaktiviert (spart Strom im Deep Sleep).
* **0xFE**: 1.8V (für 1.8V Low Power Anwendungen).
* **0xFD**: 2.7V (Empfohlen für 8 MHz Betrieb).
* **0xFC**: 4.3V (Sicherstes Level für 16 MHz / 5V Betrieb).

**Befehl (Beispiel 2.7V):**
`avrdude -c stk500v2 -P usb -p m328p -U efuse:w:0xfd:m`
