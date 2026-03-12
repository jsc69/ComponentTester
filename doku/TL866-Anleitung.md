# 🛠️ ATmega328P (TQFP32) FLASH-GUIDE (minipro v0.7.4)

### 1. CHIP-IDENTIFIKATION & TEST
Prüft, ob der Chip korrekt sitzt und die Signatur 0x1E950F liefert.
Befehl:
minipro -p "ATMEGA328P@TQFP32" -b

----------------------------------------------------------------

### 2. FUSES SETZEN (HARDWARE-SETUP)
In v0.7.4 muss jede Fuse einzeln mit -S geschrieben werden.

[ INTERNER 8 MHz TAKT (Standard ArduTester) ]
minipro -p "ATMEGA328P@TQFP32" -c fuse  -v 0xE2 -S
minipro -p "ATMEGA328P@TQFP32" -c hfuse -v 0xDE -S
minipro -p "ATMEGA328P@TQFP32" -c efuse -v 0xFD -S

[ EXTERNER 16 MHz QUARZ ]
Nur Low-Fuse ändern: minipro -p "ATMEGA328P@TQFP32" -c fuse -v 0xFF -S

----------------------------------------------------------------

### 3. FLASHEN (PROGRAMM SCHREIBEN)
Schreibt das Programm. Der Chip wird vorher automatisch gelöscht.
Befehl:
minipro -p "ATMEGA328P@TQFP32" -w firmware.hex

----------------------------------------------------------------

### 4. SCHUTZ & DIAGNOSE
[ Bootloader-Schutz ]
Sperrt Schreibzugriff auf Boot-Sektion via Lock-Bits:
minipro -p "ATMEGA328P@TQFP32" -c lock -v 0xEF -S

[ Konfiguration prüfen ]
Liest Fuses direkt ins Terminal aus:
minipro -p "ATMEGA328P@TQFP32" -r_config /dev/stdout
