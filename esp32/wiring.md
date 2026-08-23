# NoorRobot ESP32 -- Wiring Reference

This documents every physical connection the ESP32 firmware in this folder
(`esp32.ino` and its headers) actually uses. Board support ranges from a
plain ESP32 DevKit to ESP32-S3 N32R16V (see `capability.h`), so pin numbers
below assume a standard ESP32 DevKit's GPIO numbering -- if you're on a
different variant, cross-check against your specific board's pinout
diagram before wiring, since GPIO-to-physical-pin mapping is NOT identical
across ESP32 families.

---

## 1. ESP32 <-> Companion Arduino (robot control)

Used by `robot_api.h` to send movement/sensor/fan/eye commands to a
separate Arduino board that actually drives the motors, servos, distance
sensor, fan, and LED "eyes."

| ESP32 pin      | Arduino pin | Notes                        |
|----------------|-------------|-------------------------------|
| GPIO17 (TX2)   | RX          | ESP32 transmits commands here |
| GPIO16 (RX2)   | TX          | ESP32 receives replies here   |
| GND            | GND         | **Common ground is required** |

- UART2, baud **9600**, configured in `robot_api.h`:
  `ArduinoSerial.begin(9600, SERIAL_8N1, 16, 17);`
- TX/RX are crossed as usual: ESP32 TX -> Arduino RX, ESP32 RX -> Arduino TX.
- This is entirely separate from the main USB/UART0 serial below -- don't
  confuse the two when probing with a multimeter or logic analyzer.

---

## 2. ESP32 main UART0 (TX0/RX0 -- same as USB)

Used for boot logs, WiFi connection status, and the periodic IP address
print (see `esp32.ino`'s `setup()`/`loop()`).

| Signal | Notes                                                        |
|--------|---------------------------------------------------------------|
| TX0    | Same physical pin broken out on most DevKits as "TX0"/"TXD"   |
| RX0    | Same physical pin broken out on most DevKits as "RX0"/"RXD"   |
| GND    | Common ground with whatever you're monitoring this with       |

- Baud **115200**, set in `esp32.ino`: `Serial.begin(115200);`
- If flashing/monitoring over the same USB cable, you don't need to wire
  anything extra -- the onboard USB-to-UART bridge chip already connects
  to these same UART0 pins internally.
- **ESP32-S3 caveat**: if "USB CDC On Boot" is enabled in board settings,
  `Serial` is rerouted to the native USB peripheral instead of the
  physical TX0/RX0 pins. Disable that setting if you need the IP prints
  on the physical pins rather than over USB.

---

## 3. BOOT button (GPIO0) -- force WiFi setup mode

Used by `wifi_manager.h`. Holding this LOW at power-up forces the ESP32
into its WiFi setup Access Point, even if credentials are already saved.

| ESP32 pin | Connection                                    |
|-----------|------------------------------------------------|
| GPIO0     | Momentary switch to GND (pulled up internally) |

- On virtually every ESP32 DevKit this is **already wired to the onboard
  "BOOT" button** -- no extra wiring needed.
- If you're using a bare ESP32 module with no onboard button, wire GPIO0
  to GND through a momentary push-button; `pinMode(GPIO0, INPUT_PULLUP)`
  is already set in code, so no external pull-up resistor is needed.

---

## 4. SD card module (SPI) -- optional, for `storage --change`

Used by `fs_manager.h` when the storage backend is switched to `sd` via
the shell's `storage --change` command. Standard ESP32 VSPI bus.

| ESP32 pin        | SD module pin | Notes                                  |
|-------------------|---------------|------------------------------------------|
| GPIO5             | CS            | Configurable via `SD_CS_PIN` in `fs_manager.h` |
| GPIO23            | MOSI          | VSPI default                             |
| GPIO19            | MISO          | VSPI default                             |
| GPIO18            | SCK           | VSPI default                             |
| 3.3V              | VCC           | See voltage note below                   |
| GND               | GND           | Common ground                            |

- **Voltage note**: many SD breakout modules include an onboard 3.3V
  regulator and level shifter, and will happily accept 5V on VCC -- but
  some are 3.3V-only. Check your specific module's silkscreen/datasheet
  before connecting; feeding 5V logic directly into a 3.3V-only SD card
  will damage it.
- Only **CS** is dedicated to the SD card -- MOSI/MISO/SCK are the shared
  VSPI bus and would be reused if you ever add another SPI device.
- If `storage --change` reports it couldn't detect a card, double check
  this wiring and that a card is actually inserted before retrying --
  the firmware validates the card works *before* saving the preference,
  specifically so a bad connection here can't lock you out of storage.

---

## Summary diagram (text form)

```
                    +-------------------+
   Arduino  <------ | GPIO17 (TX2)      |
   (robot)  ------> | GPIO16 (RX2)      |      UART2 @ 9600
             GND <->| GND               |
                    |                   |
   USB/Debug <------| TX0 / RX0 (UART0) |      @ 115200 (or USB CDC on S3)
                    |                   |
   BOOT button ---- | GPIO0             |      pull to GND to force setup AP
                    |                   |
   SD card   <------| GPIO5  (CS)       |
             <------| GPIO23 (MOSI)     |      VSPI bus, optional
             <------| GPIO19 (MISO)     |
             <------| GPIO18 (SCK)      |
             <------| 3.3V, GND         |
                    +-------------------+
```
