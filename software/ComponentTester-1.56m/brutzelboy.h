#ifndef BRUTZELBOY_H
#define BRUTZELBOY_H

#include <stdint.h>

/* * Initialisierung der Hardware-Ports (B, C, E)
 * Wird in der main.c beim Start aufgerufen
 */
void LCD_Init(void);

/* * Sendet ein Datenbyte (Textzeichen) an den ESP32
 */
void LCD_Char(uint8_t data);

/* * Sendet ein Steuerkommando an den ESP32
 */
void LCD_test_command(uint8_t cmd);

/* * Löscht das Display (schickt ein Clear-Kommando)
 */
void LCD_Clear(void);

/* * Setzt das Cursor-Mapping (X/Y)
 */
void LCD_CharPos(uint8_t x, uint8_t y);

/* * Die "Fernbedienung": Liest ein Zeichen vom ESP32 ein,
 * falls dieser den Handshake-Pin PC5 zieht.
 */
uint8_t brutzelboy_get_key(void);

/*
 * Dummy für den Encoder-Scan (falls in config_328.h aktiviert)
 */
uint8_t brutzelboy_get_encoder(void);

#endif /* BRUTZELBOY_H */
