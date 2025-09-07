#define RX_PIN 4
#define TX_PIN 17
#define MTS_SERIAL Serial1
#define DEBUG_SERIAL Serial

#define FRAME_WORD_COUNT 10

void setup() {
  DEBUG_SERIAL.begin(115200);
  MTS_SERIAL.begin(19200, SERIAL_8N1, RX_PIN, TX_PIN);
  DEBUG_SERIAL.println("Starte ISP2 Rohdaten-Debug...");
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

          DEBUG_SERIAL.println("—— Neues Frame ——");
          for (uint8_t i = 0; i < FRAME_WORD_COUNT; i++) {
            DEBUG_SERIAL.print("Word[");
            DEBUG_SERIAL.print(i);
            DEBUG_SERIAL.print("] = ");
            DEBUG_SERIAL.print(frame[i]);
            DEBUG_SERIAL.print(" (0x");
            DEBUG_SERIAL.print(frame[i], HEX);
            DEBUG_SERIAL.println(")");
          }
          DEBUG_SERIAL.println();
        }
      }
    }
  }

  delay(5);
}
