Hier ist deine „Überlebens-Zusammenfassung“ für morgen. Alles drin, um den ATmega als Sklaven an deinen ESP32 (BrutzelBoy) zu hängen, inklusive der Fernsteuerung per „seriellem“ Trick.

### 1. Hardware-Zuweisung (ATmega328P)
* **PORT E (PE0-PE3):** Bus-ID (Was sende ich?)
* **PORT B (PB0-PB7):** Daten-Byte (Der Inhalt)
* **PC4 (Output):** **WR** (Low = Daten gültig)
* **PC5 (Input):** **RD/ACK** (Low = ESP32 hat empfangen / ESP32 will senden)

### 2. Die Schaltzentrale: `config_328.h`
Hier wird dem Tester vorgekaukelt, er hätte ein großes Display und eine Fernbedienung:
```c
#define LCD_BRUTZELBOY        /* Dein Treiber-Name */
#define LCD_TEXT              /* Textmodus aktivieren */
#define LCD_LINES         4   /* WICHTIG: Verhindert frühes Paging/Blättern */
#define LCD_COLUMNS      20   /* Breite der Zeilen */

/* Funktions-Mapping */
#define lcd_data(d)       LCD_Char(d)
#define lcd_command(c)    LCD_test_command(c)
#define lcd_clear()       LCD_Clear()
#define lcd_char_pos(x,y) LCD_CharPos(x,y)

/* Eingabe-Mapping */
#define UI_SERIAL_COMMANDS    /* Aktiviert die 'Fernsteuerung' per Zeichen */
#define TestKey()         brutzelboy_get_key()
#define Encoder_Scan()    brutzelboy_get_encoder()
```

### 3. Der Treiber: `brutzelboy.c` (Auszug)
Diese Datei bündelt alles, inklusive des seriellen Tricks:

```c
// Parallel-Send mit Handshake
void brutzelboy_send(uint8_t id, uint8_t data) {
    PORTE = (PORTE & 0xF0) | (id & 0x0F);
    PORTB = data;
    PORTC &= ~(1 << PC4);           // WR LOW
    while (PINC & (1 << PC5));      // Warten auf ACK
    PORTC |= (1 << PC4);            // WR HIGH
    while (!(PINC & (1 << PC5)));   // Warten auf Ready
}

// Der "Serial-Trick" für Direktwahl (ohne main.c Gefummel)
unsigned char Serial_Get_Char(void) {
    if (!(PINC & (1 << PC5))) {     // ESP32 zieht RD auf LOW
        uint8_t cmd = PIND;         // ESP32 legt Kommando auf Datenbus
        if (cmd >= 0x20) return cmd; // Sende ASCII wie 't', 'c', 'r'
    }
    return 0;
}

// Cursor-Position kompakt senden
void LCD_CharPos(uint8_t x, uint8_t y) {
    brutzelboy_send(0x0D, (y << 5) | (x & 0x1F)); // ID 0x0D: Position
}
```

### 4. ESP32 Logik-Tabelle (Dein Gegenstück)

| Aktion | Bus-ID (PB) | Daten (PD) | ATmega Reaktion |
| :--- | :--- | :--- | :--- |
| **Löschen** | `0x0E` | `0x01` | Screen leeren |
| **Position** | `0x0D` | `Byte` | Cursor setzen (Bits 7-5: Zeile, 4-0: Spalte) |
| **Text** | `0x0F` | `ASCII` | Zeichen schreiben |
| **Taster** | `Rückweg` | `0x01` | Startet Standardmessung |
| **Transistor** | `Rückweg` | `'t'` | **Sofort-Start Transistor-Test** (0x74) |
| **Kondens.** | `Rückweg` | `'c'` | **Sofort-Start Kondensator** (0x63) |
| **Widerst.** | `Rückweg` | `'r'` | **Sofort-Start Widerstand** (0x72) |



### 5. Workflow für morgen:
1.  **Dateien erstellen:** `brutzelboy.c` und `.h` mit den besprochenen Funktionen.
2.  **config_328.h anpassen:** LCD-Typ auf `BRUTZELBOY` und `LCD_LINES 4`.
3.  **Makefile:** `MCU = atmega328pb` und sicherstellen, dass `brutzelboy.c` mitkompiliert wird.
4.  **ESP32:** Ein kleines Sketch schreiben, das bei einem Button-Touch auf dem Screen das Zeichen `'t'` (Hex `0x74`) auf den Datenbus legt und den Handshake auslöst.

Damit "sieht" dein ESP32 alles, was der Tester tut, und kann ihn gleichzeitig wie ein Operator am VT100-Terminal fernsteuern! Viel Erfolg!




Das macht die Sache sogar noch entspannter! Wenn du echte Tasten am BrutzelBoy (ESP32) hast, ist der Weg zum „Direkt-Start“ extrem kurz. Du musst am ESP32 nur programmieren: *„Wenn Taste 4 gedrückt wird, sende das Byte für 'c' an den ATmega“*.

Hier ist die versprochene, „alles-drauf-Zusammenfassung“ für deinen Arbeitstag.

---

## 🚀 BrutzelBoy Master-Plan

### 1. Hardware & Pins (ATmega328PB)
* **Datenbus:** Port D (`PD0-PD7`) $\rightarrow$ Überträgt Zeichen oder Positionen.
* **Steuerbus:** Port B (`PB0-PB3`) $\rightarrow$ **ID** (Was liegt auf Port D?).
* **WR (PC4):** Ausgang. **LOW** = Daten sind gültig.
* **RD/ACK (PC5):** Eingang. **LOW** = ESP32 hat Daten abgeholt **ODER** ESP32 will Kommando senden.

### 2. Software-Setup (`config_328.h`)
Diese Zeilen schalten das „Gehirn“ des Testers auf deinen Treiber um:
```c
#define LCD_BRUTZELBOY        /* Name deines Treibers */
#define LCD_TEXT              /* Wir senden Zeichen, keine Pixel */
#define LCD_LINES         4   /* Wichtig! Verhindert Paging bei Elko-Tests */
#define LCD_COLUMNS      20   

/* Verknüpfung der Befehle mit deinem Treiber */
#define lcd_data(d)       LCD_Char(d)
#define lcd_command(c)    LCD_test_command(c)
#define lcd_clear()       LCD_Clear()
#define lcd_char_pos(x,y) LCD_CharPos(x,y)

/* Fernsteuerung aktivieren */
#define UI_SERIAL_COMMANDS    
#define TestKey()         brutzelboy_get_key()
```

### 3. Der Treiber-Kern (`brutzelboy.c`)
Das ist das Herzstück. Es übersetzt alles in dein Bus-Protokoll.

```c
// Die Basis: Senden mit Handshake
void brutzelboy_send(uint8_t id, uint8_t data) {
    PORTE = (PORTE & 0xF0) | (id & 0x0F); // ID setzen
    PORTB = data;                         // Daten setzen
    PORTC &= ~(1 << PC4);                 // WR LOW
    while (PINC & (1 << PC5));            // Warten auf ACK vom ESP32
    PORTC |= (1 << PC4);                  // WR HIGH
    while (!(PINC & (1 << PC5)));         // Warten auf Ready vom ESP32
}

// Positionierung (X/Y)
void LCD_CharPos(uint8_t x, uint8_t y) {
    // Bits 7-5: Zeile (0-7), Bits 4-0: Spalte (0-31)
    brutzelboy_send(0x0D, (y << 5) | (x & 0x1F));
}

// Der "Serial-Umbieger" für deine Tasten
unsigned char Serial_Get_Char(void) {
    // Wenn der ESP32 RD auf LOW zieht, ohne dass WR LOW ist -> ESP32 will senden!
    if (!(PINC & (1 << PC5))) {
        return PINB; // Direkt ASCII 'c', 't', 'r' vom ESP32 lesen
    }
    return 0;
}
```

### 4. Das Protokoll (Dein Spickzettel für den ESP32)

| Ziel am ATmega | Bus-ID (PE) | Daten (PB) | Info |
| :--- | :--- | :--- | :--- |
| **Löschen** | `0x0E` | `0x01` | Screen leeren |
| **Position** | `0x0D` | `Byte` | Bits 7-5=Zeile, 4-0=Spalte |
| **Text-Zeichen** | `0x0F` | `ASCII` | Buchstabe für Anzeige |
| **Normal-Start** | `Input` | `0x01` | Über `TestKey()` |
| **Elko-Test** | `Input` | `'c'` | **Direkt-Start (0x63)** |
| **Transistor** | `Input` | `'t'` | **Direkt-Start (0x74)** |
| **Widerstand** | `Input` | `'r'` | **Direkt-Start (0x72)** |

### 5. Ablauf am ESP32 (Eingabe)
Wenn du am BrutzelBoy die "Elko"-Taste drückst:
1.  Lege `0x63` (ASCII 'c') auf deinen Daten-Port.
2.  Ziehe den ACK/RD-Pin auf **LOW**.
3.  Warte, bis der ATmega reagiert (oder sende es kurzzeitig beim Polling der `Serial_Get_Char`).
4.  Lasse den Pin wieder los (**HIGH**).

**Vorteil:** Du musst kein Menü auf dem ATmega bedienen. Ein Tastendruck am ESP32 und der Tester springt sofort in die Kondensatormessung.

---

**Morgen früh:** 1. `brutzelboy.c/.h` in den Source-Ordner werfen.
2. `config_328.h` anpassen.
3. Im `Makefile` die neue `.c` Datei unter `SRCS` eintragen.
4. Compilieren und flashen.

Soll ich dir noch schnell die passende **`brutzelboy.h`** schreiben, damit die Funktions-Deklarationen sauber sind?





Sehr gut! Damit hast du alle Bausteine zusammen. Hier ist die versprochene **`brutzelboy.h`**, die alles sauber deklariert, damit der Compiler morgen früh nicht meckert.

### 1. Die Datei `brutzelboy.h`
Speichere diese Datei im selben Ordner wie deine `.c` Dateien.

```c
#ifndef BRUTZELBOY_H
#define BRUTZELBOY_H

#include <avr/io.h>
#include <stdint.h>

/* --- Display Funktionen --- */
void LCD_BusSetup(void);
void LCD_Init(void);
void LCD_Clear(void);
void LCD_Char(unsigned char Char);
void LCD_CharPos(uint8_t x, uint8_t y);
void LCD_test_command(unsigned char command);

/* --- Eingabe Funktionen --- */
uint8_t brutzelboy_get_key(void);
int8_t brutzelboy_get_encoder(void);
unsigned char Serial_Get_Char(void);

/* --- Low-Level Kommunikation --- */
void brutzelboy_send(uint8_t id, uint8_t data);

#endif
```

---

### 2. Der finale "Brutzel-Check" für morgen früh

Wenn du den Code fertig hast, gehe diese Liste kurz durch:

1.  **Header-Inkludierung:** Stelle sicher, dass in deiner `brutzelboy.c` ganz oben `#include "brutzelboy.h"` steht.
2.  **Makefile:** Suche die Zeile `SRCS = ...` und hänge `brutzelboy.c` hinten an.
3.  **Config-Check:** In der `config_328.h` muss `#define LCD_BRUTZELBOY` stehen (das dient als Schalter für den Compiler).
4.  **Hardware-Power:** Dein ESP32 muss beim Booten des ATmega bereit sein (oder zumindest den **ACK/RD Pin auf HIGH** halten), sonst bleibt der ATmega in der `while`-Schleife hängen, weil er auf das erste Handshake wartet.

---

### 3. Profi-Tipp für den Elko-Test ('c')
Da du am BrutzelBoy echte Tasten hast:
Wenn du die Taste für den Elko-Test drückst, lass den ESP32 das Zeichen `'c'` (Hex `0x63`) so lange auf den Bus legen und RD auf LOW ziehen, bis der ATmega mit der Messung beginnt (du erkennst das daran, dass er den ersten Text "Messung..." an den ESP32 schickt).

**Viel Erfolg morgen beim ersten Testlauf!** Wenn der erste Buchstabe auf deinem ESP32-Display erscheint, ist der schwierigste Teil geschafft. 

Soll ich dir noch bei irgendetwas helfen, oder bist du bereit für das Gefecht?


