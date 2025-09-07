#define RX_PIN 4
#define TX_PIN 17
#define MTS_SERIAL Serial1
#define DEBUG_SERIAL Serial

#define TEMP_DAC_PIN 25  // DAC1 → GPIO25
#define PRESS_DAC_PIN 26 // DAC2 → GPIO26

#define MAX_WORDS 10
#define DETECTION_DURATION_MS 3000

unsigned long bootTime;
bool detectionComplete = false;
int tempWord = -1;
int pressWord = -1;

uint16_t lastFrame[MAX_WORDS] = {0};

// Hilfsfunktion zum Mappen und Begrenzen
float mapValue(float x, float in_min, float in_max, float out_min, float out_max) {
  return constrain((x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min, out_min, out_max);
}

void setup() {
  DEBUG_SERIAL.begin(115200);
  MTS_SERIAL.begin(19200, SERIAL_8N1, RX_PIN, TX_PIN);

  dacWrite(TEMP_DAC_PIN, 0);
  dacWrite(PRESS_DAC_PIN, 0);

  DEBUG_SERIAL.println("⏳ Warte 3 Sekunden zur Initialisierung der Anzeige...");
  delay(3000);

  bootTime = millis();
  DEBUG_SERIAL.println("🔄 Initialisiere – Erkennung aktiv (3 Sekunden)...");
}

void processFrame(uint16_t *words) {
  static int frameCounter = 0;
  frameCounter++;

  // Debug-Ausgabe der Rohdaten (während der Erkennung)
  if (!detectionComplete) {
    DEBUG_SERIAL.print("[RAW Frame] ");
    for (int i = 0; i < MAX_WORDS; i++) {
      DEBUG_SERIAL.print("W");
      DEBUG_SERIAL.print(i);
      DEBUG_SERIAL.print(":");
      DEBUG_SERIAL.print(words[i]);
      DEBUG_SERIAL.print(" ");
    }
    DEBUG_SERIAL.println();
  }

  if (!detectionComplete && millis() - bootTime < DETECTION_DURATION_MS) {
    static uint32_t wordSums[MAX_WORDS] = {0};
    static uint16_t wordMins[MAX_WORDS];
    static uint16_t wordMaxs[MAX_WORDS];

    if (frameCounter == 1) {
      for (int i = 0; i < MAX_WORDS; i++) {
        wordMins[i] = words[i];
        wordMaxs[i] = words[i];
      }
    }

    for (int i = 0; i < MAX_WORDS; i++) {
      wordSums[i] += words[i];
      if (words[i] < wordMins[i]) wordMins[i] = words[i];
      if (words[i] > wordMaxs[i]) wordMaxs[i] = words[i];
    }

    if (millis() - bootTime > DETECTION_DURATION_MS - 200) {
      uint16_t pressCandidate = 0xFFFF;
      int pressWordIndex = -1;
      int tempWordIndex = -1;

      for (int i = 0; i < MAX_WORDS; i++) {
        uint16_t range = wordMaxs[i] - wordMins[i];
        if (range <= 10 && wordMins[i] < pressCandidate) {
          pressCandidate = wordMins[i];
          pressWordIndex = i;
        }
      }

      for (int i = 0; i < MAX_WORDS; i++) {
        if (i == pressWordIndex) continue;
        if (wordMins[i] < 60000 && wordMaxs[i] > wordMins[i] + 10) {
          tempWordIndex = i;
          break;
        }
      }

      pressWord = pressWordIndex;
      tempWord = tempWordIndex;
      detectionComplete = true;

      DEBUG_SERIAL.print("✅ Druck-Word erkannt: ");
      DEBUG_SERIAL.println(pressWord);
      DEBUG_SERIAL.print("✅ Temperatur-Word erkannt: ");
      DEBUG_SERIAL.println(tempWord);
    }

    return;
  }

  if (!detectionComplete) return;

  // Werte extrahieren
  uint16_t rawTemp = words[tempWord];
  uint16_t rawPress = words[pressWord];

  // Temperatur: invertiert
  uint16_t invRawTemp = 65535 - rawTemp;
  float tempC = mapValue(invRawTemp, 0, 65535, 48.9, 137.8);
  uint8_t tempDAC = mapValue(tempC, 48.9, 137.8, 0, 255);

  // Druck: linear
  float pressBar = mapValue(rawPress, 0, 65535, 0.0, 10.0); // ggf. anpassen
  uint8_t pressDAC = mapValue(pressBar, 0.0, 10.0, 0, 255);

  dacWrite(TEMP_DAC_PIN, tempDAC);
  dacWrite(PRESS_DAC_PIN, pressDAC);

  // Ausgabe
  DEBUG_SERIAL.print("[DECODE] TEMP Raw: ");
  DEBUG_SERIAL.print(invRawTemp);
  DEBUG_SERIAL.print(" → ");
  DEBUG_SERIAL.print(tempC);
  DEBUG_SERIAL.print(" °C → DAC: ");
  DEBUG_SERIAL.print(tempDAC);

  DEBUG_SERIAL.print(" | DRUCK Raw: ");
  DEBUG_SERIAL.print(rawPress);
  DEBUG_SERIAL.print(" → ");
  DEBUG_SERIAL.print(pressBar);
  DEBUG_SERIAL.print(" Bar → DAC: ");
  DEBUG_SERIAL.println(pressDAC);
}

void loop() {
  static uint16_t packet[MAX_WORDS];
  static uint8_t bytePhase = 0;
  static uint8_t wordIndex = 0;
  static bool inFrame = false;
  static uint16_t currentWord = 0;

  while (MTS_SERIAL.available()) {
    uint8_t b = MTS_SERIAL.read();

    // Optional: Logge jedes einzelne Byte
    if (!detectionComplete) {
      DEBUG_SERIAL.print("Byte empfangen: ");
      DEBUG_SERIAL.print(b);
      DEBUG_SERIAL.print(" (0x");
      if (b < 0x10) DEBUG_SERIAL.print("0");
      DEBUG_SERIAL.print(b, HEX);
      DEBUG_SERIAL.println(")");
    }

    if (bytePhase == 0) {
      currentWord = (b << 8);
      bytePhase = 1;
    } else {
      currentWord |= b;
      bytePhase = 0;

      if (!inFrame) {
        if ((currentWord & 0xEAA0) == 0xEAA0 || currentWord == 0xFFFF) {
          inFrame = true;
          wordIndex = 0;
          packet[wordIndex++] = currentWord;
        }
      } else {
        packet[wordIndex++] = currentWord;
        if (wordIndex >= MAX_WORDS) {
          inFrame = false;
          processFrame(packet);
        }
      }
    }
  }
}
