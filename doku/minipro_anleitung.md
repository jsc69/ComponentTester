# Minipro TL866 II Programmer – Befehlsreferenz

## Grundlegende Syntax
`minipro -p "CHIP@PACKAGE" [Option] [Aktion]`

## Hauptkommandos

### -r (Read)
- Liest den Inhalt eines Chips
- Speichert Daten in einer Binärdatei
- Beispiel: `minipro -p "ATMEGA328P@TQFP32" -r full_dump.bin`

### -w (Write)
- Schreibt Daten auf einen Chip
- Lädt Inhalt aus Binärdatei
- Beispiel: `minipro -p "ATMEGA328P@TQFP32" -w firmware.bin`

### -v (Verify)
- Überprüft den Chip-Inhalt
- Vergleicht mit Referenzdatei
- Beispiel: `minipro -p "ATMEGA328P@TQFP32" -v -r verify.bin`

### -e (Erase)
- Löscht den gesamten Chip-Inhalt
- Setzt Chip in Ausgangszustand
- Beispiel: `minipro -p "ATMEGA328P@TQFP32" -e`

### -L (List)
- Listet alle unterstützten Chips
- Zeigt verfügbare Programmieroptionen
- Beispiel: `minipro -L`

---

## Anwendungsbeispiele und Befehle
```bash
# Vollständiger Chip-Programmierzyklus
minipro -p "ATMEGA328P@TQFP32" -e -w firmware.bin -v

# Nur EEPROM beschreiben
minipro -p "ATMEGA328P@TQFP32" -c data -w eeprom.bin

# Chips suchen und auflisten
minipro -L
minipro -L | grep ATMEGA

# Fuse-Bits auslesen
minipro -p "ATMEGA328P@TQFP32" -c fuse -r fuses.bin

# Fuse-Bits schreiben
minipro -p "ATMEGA328P@TQFP32" -c fuse -w fuses.bin
```
---

## Fuse-Bit Übersicht für ATmega328P / ATmega328PB
```bash
# Low Fuse Byte (LFUSE)
# Standardwert: 0x62
# Interner 8MHz Oszillator: 0xE2
# Externer Oszillator: 0xFF

# High Fuse Byte (HFUSE)
# Standardwert: 0xD9
# Debuggen aktiviert: 0xD8
# EEPROM-Programmierung geschützt: 0xD1

# Extended Fuse Byte (EFUSE)
# Standardwert: 0xFF
# BOD-Level 2.7V: 0xFD
# BOD-Level 4.3V: 0xFC

# Beispiel 1: Interner 8MHz Oszillator, Standard-BOD
echo -ne '\xE2\xD9\xFF' > fuses.bin
minipro -p "ATMEGA328P@TQFP32" -c fuse -w fuses.bin

# Beispiel 2: Externer Oszillator, BOD 2.7V
echo -ne '\xFF\xD1\xFD' > fuses.bin
minipro -p "ATMEGA328P@TQFP32" -c fuse -w fuses.bin
```
---

## Zusätzliche Optionen
- `-c` Speicherbereich wählen (code, data, config, fuse)
- `-f` Dateiformat definieren (z. B. ihex)
- `-o` Überschreiben erlauben
- `-s` Silent-Modus
- `-l` Ähnliche Chipnamen listen
- `-x` Erweiterte Optionen

## Wichtige Hinweise
- Vollständigen Chipnamen mit Gehäusetyp angeben
- Vorsicht bei Fuse-Bit-Änderungen
- Hilfe: `minipro -h` oder `man minipro`
