#include <avr/io.h>
#include "config.h"

// Initialisierung der Ports für den Bus
void LCD_Init(void) {
    // Port B (Daten) als Ausgang
    DDRB = 0xFF;
    // Port E (ID) PE0-PE3 als Ausgang
    DDRE |= 0x0F;
    // Port C: PC4 (WR) als Ausgang, PC5 (RD) als Eingang
    DDRC |= (1 << PC4);
    DDRC &= ~(1 << PC5);
    // Initial-Pegel: WR auf High
    PORTC |= (1 << PC4);
}

// Die zentrale Sendefunktion
void LCD_Char(unsigned char data) {
    // ID 0 = Text/Daten (Beispiel)
    PORTE = (PORTE & 0xF0) | 0x00;
    PORTB = data;

    // Handshake
    PORTC &= ~(1 << PC4);           // WR Low
    while (PINC & (1 << PC5));      // Warten auf ACK vom ESP32 (RD Low)
    PORTC |= (1 << PC4);            // WR High
    while (!(PINC & (1 << PC5)));   // Warten auf Freigabe (RD High)
}

// Dummy für Befehle
void LCD_test_command(unsigned char cmd) {
    // Gleiche Logik wie Char, nur mit ID 1 (Befehl)
    PORTE = (PORTE & 0xF0) | 0x01;
    PORTB = cmd;
    // ... Handshake wie oben ...
}

// Deine "Fernsteuerung" vom ESP32
unsigned char brutzelboy_get_key(uint16_t Timeout, uint8_t Mode) {
    // Wenn RD (PC5) vom ESP32 auf LOW gezogen wird (außerhalb eines Sendevorgangs)
    // interpretiert der ATmega das als Tastendruck
    if (!(PINC & (1 << PC5))) {
        return PINB; // Das Zeichen vom Datenbus lesen
    }
    return 0;
}

void LCD_BusSetup(void) {
    /* Hier kommt dein Code rein, um die Pins für das Display zu initialisieren */
}

void LCD_Clear(void) {
    /* Hier kommt dein Code rein, um das Display zu löschen */
}

void LCD_CharPos(void) {

}

void LCD_ClearLine(void) {

}
