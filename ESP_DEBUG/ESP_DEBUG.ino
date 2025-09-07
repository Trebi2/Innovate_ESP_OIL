#define RX_PIN 4
#define TX_PIN 17
#define MTS_SERIAL Serial1
#define DEBUG_SERIAL Serial

void setup() {
  DEBUG_SERIAL.begin(115200);
  MTS_SERIAL.begin(19200, SERIAL_8N1, RX_PIN, TX_PIN);
  DEBUG_SERIAL.println("🔎 Serieller Byte-Monitor gestartet...");
}

void loop() {
  while (MTS_SERIAL.available()) {
    uint8_t b = MTS_SERIAL.read();
    
    DEBUG_SERIAL.print("Empfangenes Byte: ");
    DEBUG_SERIAL.print(b);         // Dezimal
    DEBUG_SERIAL.print(" (0x");
    if (b < 0x10) DEBUG_SERIAL.print("0");
    DEBUG_SERIAL.print(b, HEX);    // Hex
    DEBUG_SERIAL.println(")");
  }
}
