/* ************************************************************************
 * ComponentTester-1.56m Wrapper für Arduino IDE
 * * Projekt: Transistortester (Markus Reschke / Karl-Heinz Kübbeler)
 * Ziel-Hardware: ATmega328PB (oder ATmega328P)
 * ************************************************************************ */

/* * WICHTIGE EINSTELLUNGEN IN DER ARDUINO IDE:
 * 1. Board-Verwalter: "MiniCore" von MCUdude muss installiert sein.
 * (URL: https://mcudude.github.io/MiniCore/package_MCUdude_MiniCore_index.json)
 * 2. Werkzeuge -> Board: "ATmega328"
 * 3. Werkzeuge -> Variant: "328PB" (Wichtig für den PB-Chip!)
 * 4. Werkzeuge -> Clock: "Internal 8 MHz"
 * 5. Werkzeuge -> Compiler LTO: "LTO enabled" (hilft Platz zu sparen)
 * * HINWEIS ZUM KOMPILIEREN:
 * Da das Projekt eine eigene 'main()'-Funktion in 'main.c' hat, kann es
 * beim Linken zu einem Fehler "multiple definition of 'main'" kommen.
 * Sollte dies passieren, benenne im Tab 'main.c' die Funktion:
 * 'int main(void)'  um in  'void setup()'
 * Die 'void loop()' hier unten bleibt dann einfach leer.
 */

void setup() {
  // Bleibt leer, wenn main.c unverändert bleibt 
  // ODER ruft die umbenannte main-Funktion auf.
}

void loop() {
  // Der Tester nutzt eine eigene Endlosschleife in der main.c
}