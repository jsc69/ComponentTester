#!/bin/bash

# minipro Interactive Menu
# Interaktives Menü für TL866 II Plus Programmer

CHIP=""
COLOR_RESET="\e[0m"
COLOR_GREEN="\e[32m"
COLOR_YELLOW="\e[33m"
COLOR_RED="\e[31m"
COLOR_BLUE="\e[34m"

# Funktion: Chip auswählen
select_chip() {
    echo -e "${COLOR_BLUE}=== Chip auswählen ===${COLOR_RESET}"
    echo "Verfügbare ATmega328 Varianten:"
    echo "1) ATMEGA328@DIP28"
    echo "2) ATMEGA328@TQFP32"
    echo "3) ATMEGA328@MLF32"
    echo "4) ATMEGA328P@DIP28"
    echo "5) ATMEGA328P@TQFP32"
    echo "6) ATMEGA328P@MLF32"
    echo "7) ATMEGA328PB@TQFP32"
    echo "8) Anderen Chip eingeben"
    echo "9) Liste aller Chips anzeigen"
    read -p "Auswahl: " choice
    
    case $choice in
        1) CHIP="ATMEGA328@DIP28" ;;
        2) CHIP="ATMEGA328@TQFP32" ;;
        3) CHIP="ATMEGA328@MLF32" ;;
        4) CHIP="ATMEGA328P@DIP28" ;;
        5) CHIP="ATMEGA328P@TQFP32" ;;
        6) CHIP="ATMEGA328P@MLF32" ;;
        7) CHIP="ATMEGA328PB@TQFP32" ;;
        8) 
            read -p "Chip-Name eingeben: " CHIP
            ;;
        9)
            minipro -l | less
            select_chip
            return
            ;;
        *) 
            echo -e "${COLOR_RED}Ungültige Auswahl${COLOR_RESET}"
            select_chip
            return
            ;;
    esac
    
    echo -e "${COLOR_GREEN}Chip ausgewählt: $CHIP${COLOR_RESET}"
}

# Funktion: Chip auslesen
read_chip() {
    read -p "Dateiname für Backup [backup.hex]: " filename
    filename=${filename:-backup.hex}
    echo -e "${COLOR_YELLOW}Lese Chip aus...${COLOR_RESET}"
    minipro -p "$CHIP" -r "$filename"
    if [ $? -eq 0 ]; then
        echo -e "${COLOR_GREEN}Erfolgreich gespeichert: $filename${COLOR_RESET}"
    else
        echo -e "${COLOR_RED}Fehler beim Auslesen${COLOR_RESET}"
    fi
}

# Funktion: Chip flashen
write_chip() {
    read -p "Dateiname zum Flashen: " filename
    if [ ! -f "$filename" ]; then
        echo -e "${COLOR_RED}Datei nicht gefunden: $filename${COLOR_RESET}"
        return
    fi
    
    echo "Optionen:"
    echo "1) Normal flashen (ohne Chip-Erase)"
    echo "2) Mit Chip-Erase (löscht alles!)"
    read -p "Auswahl: " erase_choice
    
    if [ "$erase_choice" == "2" ]; then
        echo -e "${COLOR_RED}WARNUNG: Chip wird komplett gelöscht (inkl. Bootloader)!${COLOR_RESET}"
        read -p "Fortfahren? (ja/nein): " confirm
        if [ "$confirm" != "ja" ]; then
            echo "Abgebrochen."
            return
        fi
        echo -e "${COLOR_YELLOW}Flashe mit Chip-Erase...${COLOR_RESET}"
        minipro -p "$CHIP" -e -w "$filename"
    else
        echo -e "${COLOR_YELLOW}Flashe ohne Chip-Erase...${COLOR_RESET}"
        minipro -p "$CHIP" -w "$filename"
    fi
    
    if [ $? -eq 0 ]; then
        echo -e "${COLOR_GREEN}Erfolgreich geflasht${COLOR_RESET}"
    else
        echo -e "${COLOR_RED}Fehler beim Flashen${COLOR_RESET}"
    fi
}

# Funktion: Chip löschen
erase_chip() {
    echo -e "${COLOR_RED}WARNUNG: Chip wird komplett gelöscht (inkl. Bootloader)!${COLOR_RESET}"
    read -p "Fortfahren? (ja/nein): " confirm
    if [ "$confirm" == "ja" ]; then
        echo -e "${COLOR_YELLOW}Lösche Chip...${COLOR_RESET}"
        minipro -p "$CHIP" -e
        if [ $? -eq 0 ]; then
            echo -e "${COLOR_GREEN}Chip gelöscht${COLOR_RESET}"
        else
            echo -e "${COLOR_RED}Fehler beim Löschen${COLOR_RESET}"
        fi
    else
        echo "Abgebrochen."
    fi
}

# Funktion: Fuse-Bits decodieren (ATmega328/328P/328PB)
decode_fuses() {
    local low=$1
    local high=$2
    local ext=$3
    
    echo ""
    echo -e "${COLOR_BLUE}=== Low Fuse (0x$(printf %02x $low)) ===${COLOR_RESET}"
    
    # CKSEL[3:0] & SUT[1:0]
    local cksel=$((low & 0x0F))
    case $cksel in
        0x00|0x01|0x02|0x03) echo "  Clock: External Clock" ;;
        0x04|0x05) echo "  Clock: Internal 128kHz RC" ;;
        0x06|0x07) echo "  Clock: External Crystal/Resonator (Low Freq)" ;;
        0x08|0x09) echo "  Clock: External Crystal/Resonator (Med Freq)" ;;
        0x0A|0x0B|0x0C|0x0D|0x0E|0x0F) echo "  Clock: External Crystal/Resonator (High Freq)" ;;
    esac
    
    # CKOUT
    if [ $((low & 0x40)) -eq 0 ]; then
        echo "  CKOUT: Enabled (Clock output on PORTB0)"
    else
        echo "  CKOUT: Disabled"
    fi
    
    # CKDIV8
    if [ $((low & 0x80)) -eq 0 ]; then
        echo "  CKDIV8: Enabled (Clock divided by 8)"
    else
        echo "  CKDIV8: Disabled (Full speed)"
    fi
    
    echo ""
    echo -e "${COLOR_BLUE}=== High Fuse (0x$(printf %02x $high)) ===${COLOR_RESET}"
    
    # BOOTRST
    if [ $((high & 0x01)) -eq 0 ]; then
        echo "  BOOTRST: Boot to Bootloader"
    else
        echo "  BOOTRST: Boot to Application (0x0000)"
    fi
    
    # BOOTSZ[1:0]
    local bootsz=$(((high >> 1) & 0x03))
    case $bootsz in
        0) echo "  BOOTSZ: 2048 words (4096 bytes)" ;;
        1) echo "  BOOTSZ: 1024 words (2048 bytes)" ;;
        2) echo "  BOOTSZ: 512 words (1024 bytes)" ;;
        3) echo "  BOOTSZ: 256 words (512 bytes)" ;;
    esac
    
    # EESAVE
    if [ $((high & 0x08)) -eq 0 ]; then
        echo "  EESAVE: EEPROM preserved during chip erase"
    else
        echo "  EESAVE: EEPROM erased during chip erase"
    fi
    
    # WDTON
    if [ $((high & 0x10)) -eq 0 ]; then
        echo "  WDTON: Watchdog always on"
    else
        echo "  WDTON: Watchdog controllable"
    fi
    
    # SPIEN
    if [ $((high & 0x20)) -eq 0 ]; then
        echo "  SPIEN: SPI programming enabled"
    else
        echo "  SPIEN: SPI programming DISABLED"
    fi
    
    # DWEN
    if [ $((high & 0x40)) -eq 0 ]; then
        echo "  DWEN: debugWIRE enabled"
    else
        echo "  DWEN: debugWIRE disabled"
    fi
    
    # RSTDISBL
    if [ $((high & 0x80)) -eq 0 ]; then
        echo "  RSTDISBL: Reset pin DISABLED (PC6 as I/O)"
    else
        echo "  RSTDISBL: Reset pin enabled"
    fi
    
    echo ""
    echo -e "${COLOR_BLUE}=== Extended Fuse (0x$(printf %02x $ext)) ===${COLOR_RESET}"
    
    # BODLEVEL[2:0]
    local bodlevel=$((ext & 0x07))
    case $bodlevel in
        7) echo "  BOD: Disabled" ;;
        6) echo "  BOD: 1.8V" ;;
        5) echo "  BOD: 2.7V" ;;
        4) echo "  BOD: 4.3V" ;;
        *) echo "  BOD: Reserved/Unknown" ;;
    esac
}

# Funktion: Fuses lesen
read_fuses() {
    echo -e "${COLOR_YELLOW}Lese Fuse Bits...${COLOR_RESET}"
    
    # Temporäre Datei verwenden für saubere Ausgabe
    tmpfile=$(mktemp)
    minipro -p "$CHIP" -c config -r "$tmpfile" 2>&1 | grep -v "^Found\|^Device\|^Serial\|^USB\|^Warning"
    
    if [ -f "$tmpfile" ]; then
        # Fuse-Werte auslesen
        local fuses=($(hexdump -v -e '1/1 "%d "' "$tmpfile"))
        local low=${fuses[0]:-0}
        local high=${fuses[1]:-0}
        local ext=${fuses[2]:-0}
        
        echo ""
        echo -e "${COLOR_GREEN}Fuse Bits:${COLOR_RESET}"
        printf "  Low Fuse:  0x%02x (%d)\n" $low $low
        printf "  High Fuse: 0x%02x (%d)\n" $high $high
        printf "  Ext Fuse:  0x%02x (%d)\n" $ext $ext
        
        # Nur für ATmega328 Familie decodieren
        if [[ "$CHIP" =~ ATMEGA328 ]]; then
            decode_fuses $low $high $ext
        fi
        
        rm "$tmpfile"
    else
        echo -e "${COLOR_RED}Fehler beim Lesen der Fuses${COLOR_RESET}"
    fi
}

# Funktion: Fuses schreiben
write_fuses() {
    echo -e "${COLOR_RED}WARNUNG: Falsche Fuse-Einstellungen können den Chip unbrauchbar machen!${COLOR_RESET}"
    read -p "Dateiname mit Fuse-Daten: " filename
    if [ ! -f "$filename" ]; then
        echo -e "${COLOR_RED}Datei nicht gefunden: $filename${COLOR_RESET}"
        return
    fi
    
    read -p "Wirklich Fuses schreiben? (ja/nein): " confirm
    if [ "$confirm" == "ja" ]; then
        echo -e "${COLOR_YELLOW}Schreibe Fuse Bits...${COLOR_RESET}"
        minipro -p "$CHIP" -c config -w "$filename"
        if [ $? -eq 0 ]; then
            echo -e "${COLOR_GREEN}Fuses geschrieben${COLOR_RESET}"
        else
            echo -e "${COLOR_RED}Fehler beim Schreiben${COLOR_RESET}"
        fi
    else
        echo "Abgebrochen."
    fi
}

# Funktion: EEPROM lesen
read_eeprom() {
    read -p "Dateiname für EEPROM [eeprom.bin]: " filename
    filename=${filename:-eeprom.bin}
    echo -e "${COLOR_YELLOW}Lese EEPROM...${COLOR_RESET}"
    minipro -p "$CHIP" -c eeprom -r "$filename"
    if [ $? -eq 0 ]; then
        echo -e "${COLOR_GREEN}Erfolgreich gespeichert: $filename${COLOR_RESET}"
    else
        echo -e "${COLOR_RED}Fehler beim Auslesen${COLOR_RESET}"
    fi
}

# Funktion: EEPROM schreiben
write_eeprom() {
    read -p "Dateiname für EEPROM: " filename
    if [ ! -f "$filename" ]; then
        echo -e "${COLOR_RED}Datei nicht gefunden: $filename${COLOR_RESET}"
        return
    fi
    
    echo -e "${COLOR_YELLOW}Schreibe EEPROM...${COLOR_RESET}"
    minipro -p "$CHIP" -c eeprom -w "$filename"
    if [ $? -eq 0 ]; then
        echo -e "${COLOR_GREEN}EEPROM geschrieben${COLOR_RESET}"
    else
        echo -e "${COLOR_RED}Fehler beim Schreiben${COLOR_RESET}"
    fi
}

# Hauptmenü
main_menu() {
    while true; do
        clear
        echo -e "${COLOR_BLUE}╔════════════════════════════════════════╗${COLOR_RESET}"
        echo -e "${COLOR_BLUE}║   TL866 II Plus - minipro Menü        ║${COLOR_RESET}"
        echo -e "${COLOR_BLUE}╚════════════════════════════════════════╝${COLOR_RESET}"
        
        if [ -z "$CHIP" ]; then
            echo -e "${COLOR_RED}Kein Chip ausgewählt!${COLOR_RESET}"
        else
            echo -e "${COLOR_GREEN}Aktueller Chip: $CHIP${COLOR_RESET}"
        fi
        
        echo ""
        echo "1)  Chip auswählen"
        echo "2)  Chip auslesen (Flash)"
        echo "3)  Chip flashen"
        echo "4)  Chip löschen"
        echo "5)  Fuse Bits lesen"
        echo "6)  Fuse Bits schreiben"
        echo "7)  EEPROM lesen"
        echo "8)  EEPROM schreiben"
        echo "9)  Alle Chips auflisten"
        echo "0)  Beenden"
        echo ""
        read -p "Auswahl: " choice
        
        case $choice in
            1) select_chip ;;
            2) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    read_chip
                fi
                ;;
            3) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    write_chip
                fi
                ;;
            4) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    erase_chip
                fi
                ;;
            5) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    read_fuses
                fi
                ;;
            6) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    write_fuses
                fi
                ;;
            7) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    read_eeprom
                fi
                ;;
            8) 
                if [ -z "$CHIP" ]; then
                    echo -e "${COLOR_RED}Bitte zuerst Chip auswählen!${COLOR_RESET}"
                else
                    write_eeprom
                fi
                ;;
            9) minipro -l | less ;;
            0) 
                echo "Auf Wiedersehen!"
                exit 0
                ;;
            *) 
                echo -e "${COLOR_RED}Ungültige Auswahl${COLOR_RESET}"
                ;;
        esac
        
        echo ""
        read -p "Drücke Enter um fortzufahren..."
    done
}

# Programm starten
main_menu