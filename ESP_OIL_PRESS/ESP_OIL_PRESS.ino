#define RX_PIN 4
#define TX_PIN 17
#define MTS_SERIAL Serial1
#define DEBUG_SERIAL Serial

#define TEMP_DAC_PIN 26  // DAC2 = GPIO 26
#define PRESS_DAC_PIN 25 // DAC1 = GPIO 25

#define MAX_WORDS 10
uint16_t lastValues[MAX_WORDS] = {0};
unsigned long lastStatusTime = 0;

void setup() {
  DEBUG_SERIAL.begin(115200);
  dacWrite(TEMP_DAC_PIN, 0);
  dacWrite(PRESS_DAC_PIN, 0);
  DEBUG_SERIAL.println("⏳ Warte 3 Sekunden zur Initialisierung der Anzeige...");
  delay(3000);
  DEBUG_SERIAL.println("Starte ISP2-Frame mit Temperatur- und Druckausgabe...");
  MTS_SERIAL.begin(19200, SERIAL_8N1, RX_PIN, TX_PIN);
  DEBUG_SERIAL.println("✅ Initialisierung abgeschlossen. Tracker aktiv.");
}

void loop() {
  static uint16_t packet[MAX_WORDS];
  static uint8_t wordIndex = 0;
  static bool inFrame = false;
  static uint8_t bytePhase = 0;
  static uint16_t currentWord = 0;

  if (millis() - lastStatusTime > 10000) {
    DEBUG_SERIAL.println("⏳ Tracker aktiv – keine Änderungen erkannt.");
    lastStatusTime = millis();
  }

  while (MTS_SERIAL.available()) {
    uint8_t b = MTS_SERIAL.read();

    if (bytePhase == 0) {
      currentWord = (b << 8);
      bytePhase = 1;
    } else {
      currentWord |= b;
      bytePhase = 0;

      if (!inFrame) {
        if (currentWord == 0xFFFF) {
          inFrame = true;
          wordIndex = 0;
          packet[wordIndex++] = currentWord;
        }
      } else {
        packet[wordIndex++] = currentWord;

        if (wordIndex >= MAX_WORDS) {
          inFrame = false;

          // — FRAME AUSGABE —
          DEBUG_SERIAL.print("[Frame]");
          for (uint8_t i = 0; i < MAX_WORDS; i++) {
            DEBUG_SERIAL.print(" W");
            DEBUG_SERIAL.print(i);
            DEBUG_SERIAL.print(":");
            DEBUG_SERIAL.print(packet[i]);
          }
          DEBUG_SERIAL.println();

          // — TEMPERATUR (invertiert) —
          uint16_t tempInvRaw = packet[1];
          uint16_t tempRaw = 65535 - tempInvRaw;
          float tempC = ((65280.0 - tempRaw) / (65280.0 - 39168.0)) * (108.9 - 48.9) + 48.9;
          int dacTemp = map(tempC * 10, 490, 1380, 0, 255);
          dacTemp = constrain(dacTemp, 0, 255);
          dacWrite(TEMP_DAC_PIN, dacTemp);

          DEBUG_SERIAL.print(" → TEMP Raw: ");
          DEBUG_SERIAL.print(tempRaw);
          DEBUG_SERIAL.print(" → °C: ");
          DEBUG_SERIAL.print(tempC, 1);
          DEBUG_SERIAL.print(" → DAC2: ");
          DEBUG_SERIAL.println(dacTemp);

          // — DRUCK —
          uint16_t pressRaw = packet[2];
          // Beispiel-Skalierung: 37791 = 0 Bar, 30000 = 10 Bar (angepasst werden!)
          float bar = ((float)pressRaw - 37791.0) / (30000.0 - 37791.0) * 10.0;
          bar = constrain(bar, 0.0, 10.0);

          int dacPress = map(bar * 10, 0, 100, 0, 255);
          dacPress = constrain(dacPress, 0, 255);
          dacWrite(PRESS_DAC_PIN, dacPress);

          DEBUG_SERIAL.print(" → PRESS Raw: ");
          DEBUG_SERIAL.print(pressRaw);
          DEBUG_SERIAL.print(" → Bar: ");
          DEBUG_SERIAL.print(bar, 2);
          DEBUG_SERIAL.print(" → DAC1: ");
          DEBUG_SERIAL.println(dacPress);
        }
      }
    }
  }

  delay(5);
}
