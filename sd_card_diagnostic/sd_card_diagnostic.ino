// sd_card_diagnostic.ino
//
// Standalone SD card test -- completely isolated from NoorRobot-system's
// shell/WiFi/Lua stack, so a failure here can ONLY be the SD card, wiring,
// or module itself.
//
// v2: retries continuously in loop() instead of once in setup(). This is a
// no-multimeter continuity test: with the board running and Serial Monitor
// open, gently wiggle ONE connection at a time (start with CS, then SCK,
// MOSI, MISO, VCC, GND, then the card seating itself) while leaving all
// others completely still. The instant a wire is the culprit, you'll see
// output flip from FAIL to PASS while you're touching it -- that pinpoints
// the exact bad connection without needing to measure anything.
//
// Test order suggestion (worst offenders first):
//   1. CS (GPIO5)     -- wiggle at both the ESP32 pin and the module pin
//   2. SCK (GPIO18)
//   3. MOSI (GPIO23)
//   4. MISO (GPIO19)
//   5. GND            -- easy to overlook, silently breaks everything
//   6. VCC
//   7. Push the card itself firmly, try re-seating it fully
//
// Flash this to a board with nothing else running, open Serial Monitor at
// 115200 baud, and start wiggling while watching the output.

#include <SPI.h>
#include <SD.h>

#define SD_CS_PIN 5
#define SD_SCK  18
#define SD_MISO 19
#define SD_MOSI 23

int attempt = 0;

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("=== SD Card Diagnostic v2 (continuous retry) ===");
  Serial.println("Wiggle ONE wire at a time while watching this output.");
  Serial.println("Order: CS -> SCK -> MOSI -> MISO -> GND -> VCC -> reseat card");
  Serial.println();
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS_PIN);
}

void loop() {
  attempt++;
  bool ok = SD.begin(SD_CS_PIN, SPI, 400000); // slow + forgiving on purpose
  Serial.printf("[attempt %4d] SD.begin(): %s\n", attempt, ok ? "PASS ***" : "fail");
  if (ok) {
    uint8_t cardType = SD.cardType();
    Serial.print("  -> Card type: ");
    switch (cardType) {
      case CARD_NONE:  Serial.println("NONE"); break;
      case CARD_MMC:   Serial.println("MMC"); break;
      case CARD_SD:    Serial.println("SDSC"); break;
      case CARD_SDHC:  Serial.println("SDHC/SDXC"); break;
      default:         Serial.println("UNKNOWN"); break;
    }
    Serial.printf("  -> Card size: %llu MB\n", SD.cardSize() / (1024 * 1024));
    Serial.println("  -> FOUND IT. Whatever you just touched/wiggled is the bad connection.");
    Serial.println("  -> Hold it in that exact position and note which wire/pin it was.");
    SD.end();
  }
  delay(300); // slow enough to read, fast enough to catch a brief good contact
}
