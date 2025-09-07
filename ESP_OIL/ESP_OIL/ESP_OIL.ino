#define RX_PIN 4
#define TX_PIN 17

#define TEMP_DAC_PIN 26  // DAC2 auf GPIO 26
#define PRESS_DAC_PIN 25 // DAC1 auf GPIO 25

#define MTS_SERIAL Serial1
#define DEBUG_SERIAL Serial

#define FRAME_WORD_COUNT 10

void setup() {
  delay 3000;
  DEBUG_SERIAL.begin(115200);
  MTS_SERIAL.begin(19200, SERIAL_8N1, RX_PIN, TX_PIN);
  DEBUG_SERIAL.println("Starte Temperatur-Decoder (W1-basiert)...");
}

void loop() {
  static uint8_t bytePhase = 0;
  static uint16_t currentWord = 0;
  static uint16_t frame[FRAME_WORD_COUNT];
  static uint8_t wordIndex = 0;
  static bool inFrame = false;

  while (MTS_SERIAL.available()) {
    uint8_t b = MTS_SERIAL.read();

    if (bytePhase == 0) {
      currentWord = (b << 8);
      bytePhase = 1;
    } else {
      currentWord |= b;
      bytePhase = 0;

      if (!inFrame) {
        if ((currentWord & 0xEAA0) == 0xEAA0) {
          inFrame = true;
          wordIndex = 0;
          frame[wordIndex++] = currentWord;
        }
      } else {
        frame[wordIndex++] = currentWord;

        if (wordIndex >= FRAME_WORD_COUNT) {
          inFrame = false;

          // Temperatur liegt in Word[1]
          uint16_t raw = frame[1];

          // Neue Kalibrierung: 65280 = 48.9°C, 39168 = 108.9°C
          float tempC = ((65280.0 - raw) / (65280.0 - 39168.0)) * 60.0 + 48.9;

          // Sicherheitsbegrenzung
          tempC = constrain(tempC, 0.0, 150.0);

          // DAC-Wert berechnen: 0–150°C → 0–255
          int dacValue = map(tempC * 10, 490, 1380, 0, 255);
          dacWrite(TEMP_DAC_PIN, dacValue);

          // Debug-Ausgabe
          DEBUG_SERIAL.print("[TEMP] Raw(W1): ");
          DEBUG_SERIAL.print(raw);
          DEBUG_SERIAL.print(" → °C: ");
          DEBUG_SERIAL.print(tempC, 1);
          DEBUG_SERIAL.print(" → DAC: ");
          DEBUG_SERIAL.println(dacValue);
        }
      }
    }
  }

  delay(5);
}
