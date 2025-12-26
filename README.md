# ESP P.T.D

ESP P.T.D is an ESP32‑based portable wireless toolkit with a button‑driven TFT UI. It lets you explore common 2.4 GHz Wi‑Fi and BLE behaviors from a compact handheld device for **lab and educational** use only. [page:1][web:49]

> ⚠️ Use this firmware only on networks and devices you own or have explicit permission to test. Misuse of deauth, beacon spam, or fake portals may be illegal.

---

## Features

### Wi‑Fi tools
- AP scanner: Lists nearby access points (SSID, BSSID, RSSI, channel) on the TFT.  
- Random SSID spam: Sends beacon frames with randomly generated SSIDs like `ESP_PTD_xx`.  
- Funny SSID spam: Sends beacon frames using a built‑in list (FREE_WIFI, NSA_VAN_13, etc.).  
- Clone SSID spam: Replays SSIDs from the last scan as fake AP beacons.  
- Targeted deauth: Sends deauth/disassoc frames to a selected AP’s BSSID on its channel (deauther‑style burst). [file:1][web:53]

### BLE tools
- BLE sniffer: Uses NimBLE scan callbacks to show live advertising packets and device addresses in a scrolling log.  
- BLE beacon spam: Rapidly advertises random BLE names like `ESP_BLE_xx`.  
- BLE HID macro: Exposes a HID keyboard named `ESP_PTD_HID` and types a short text payload once when connected. [file:1][web:51]

### Evil Portal
- Fake open AP: Starts an access point called `Free_Public_WiFi` with DNS redirection to the ESP32.  
- Captive portal page: Serves `/index.html` from LittleFS and captures `user` and `pass` POST fields, showing the last credentials on the TFT and over Serial. [file:1]

---

## Hardware

- MCU: ESP32 dev board (generic ESP32‑WROOM style).  
- Display: 3.5" SPI TFT, ILI9488 driver, 480×320, using TFT_eSPI.  
- Inputs: 3 push buttons (UP/DOWN/OK) on GPIO 13/12/14 (active LOW). [file:1][web:30]

### TFT wiring (ILI9488 SPI)

| TFT pin       | ESP32 pin          |
|---------------|--------------------|
| VCC           | 3V3                |
| GND           | GND                |
| SCK / CLK     | GPIO 18 (VSPI SCK) |
| MOSI / SDA    | GPIO 23 (VSPI MOSI)|
| MISO          | not used (or GPIO 19 if required) |
| CS            | GPIO 5             |
| DC / RS       | GPIO 2             |
| RST           | GPIO 4 (or 3V3 for auto‑reset) |
| LED / BL      | 3V3 via resistor or transistor |

### Buttons

| Function | GPIO | Notes          |
|---------|------|----------------|
| UP      | 13   | `INPUT_PULLUP` |
| DOWN    | 12   | `INPUT_PULLUP` |
| OK      | 14   | `INPUT_PULLUP` |

---

## Software & libraries

Single Arduino sketch: `progz_marauder.ino`. [page:1]

Required libraries:
- `TFT_eSPI` (2.5.43, custom config for ILI9488)  
- `WiFi` / `esp_wifi.h`  
- `NimBLE-Arduino`  
- `DNSServer`  
- `AsyncTCP`  
- `ESPAsyncWebServer`  
- `LittleFS` / `LittleFS_esp32`  
- `ESP32-NimBLE-Keyboard` (`BleKeyboard`) [file:1][web:46]

### TFT_eSPI configuration

The repo includes `TFT_eSPI-2.5.43.zip` with a tested configuration.

1. Install TFT_eSPI (Library Manager or ZIP).  
2. Replace its `User_Setup.h` with the one from this project, or configure the driver as ILI9488 and set pins as in the wiring table.  
3. Select 480×320 and rotation to match your hardware. [page:1][web:36]

---

## Building & flashing

1. Clone this repo or download as ZIP.  
2. Open `progz_marauder.ino` in Arduino IDE.  
3. Install ESP32 board support in Board Manager and select **ESP32 Dev Module** (or equivalent).  
4. Install the required libraries from the list above.  
5. Upload the sketch to your ESP32.  
6. Use the ESP32 LittleFS uploader (or PlatformIO *Upload File System Image*) to flash the `/data` folder containing `index.html` for the Evil Portal. [file:1][web:51]

---

## Controls

- UP (GPIO 13): Move selection up in menus.  
- DOWN (GPIO 12): Move selection down in menus.  
- OK (GPIO 14): Enter / confirm / back on most screens.  
- Serial shortcuts:  
  - On main menu, send `1`–`4` over Serial to jump to Settings / WiFi tools / BLE tools / Evil Portal.  
  - `0`: Works as “Back” in many screens. [file:1][web:49]
