# ESP P.T.D

**ESP32-based portable wireless research toolkit with a TFT display and single-button interface.**

Built for hands-on learning of 2.4 GHz Wi-Fi and Bluetooth Low Energy behaviours in controlled lab environments.

> ⚠️ **Legal Notice** — This firmware is intended exclusively for use on networks and devices you own or have written authorisation to test. Deploying deauthentication frames, beacon floods, BLE jamming, or captive portals against infrastructure or devices without explicit permission is illegal in most jurisdictions. The author accepts no liability for misuse.

---

## Table of Contents

- [Features](#features)
- [Hardware Requirements](#hardware-requirements)
- [Wiring](#wiring)
- [Software & Libraries](#software--libraries)
- [TFT\_eSPI Configuration](#tft_espi-configuration)
- [Building & Flashing](#building--flashing)
- [LittleFS — Uploading the Portal Page](#littlefs--uploading-the-portal-page)
- [Controls](#controls)
- [License](#license)

---

## Features

### Wi-Fi Tools

| Feature | Description |
|---|---|
| AP Scanner | Scans and lists nearby access points — SSID, BSSID, RSSI, channel, encryption. Sorted by signal strength, name, or open-first (configurable). |
| Random SSID Spam | Broadcasts beacon frames with randomly generated SSIDs. |
| Funny SSID Spam | Broadcasts a built-in list of humorous SSIDs (14 entries). |
| Clone All APs | Replays every SSID from the last scan as beacon frames simultaneously. |
| Clone Target AP | Select a single AP from the scan list and spam only that SSID. |
| Targeted Deauth | Sends deauthentication and disassociation frames to a selected AP's BSSID on its channel. Burst size is configurable (Safe / Aggressive). |

### BLE Tools

| Feature | Description |
|---|---|
| BLE Sniffer | Live NimBLE scan — displays advertising devices with address, name, and RSSI signal bar. |
| Flipper Scanner | Dedicated Flipper Zero detection. Identifies devices by BLE name prefix and advertised service UUIDs. Detected units highlighted in red with a `[FLIPPER]` tag. |
| Beacon Spam | Rapidly advertises random BLE device names — floods nearby BLE discovery lists. |
| Apple Spam | Sends Apple Proximity Pairing manufacturer data packets. Triggers popup notifications on nearby iPhones and iPads. |
| BT Jammer | Minimal-payload maximum-rate advertising burst across all three BLE channels. Disrupts nearby BLE device discovery. |

### Evil Portal

| Feature | Description |
|---|---|
| Captive Portal AP | Opens a fake access point (`Free_Public_WiFi`) with DNS redirection. All HTTP traffic is redirected to the ESP32. |
| Credential Capture | Serves `index.html` from LittleFS. Captures submitted credentials and logs them to the TFT display, Serial, and `/creds.txt` on flash. |
| Credential Dump | Access `http://192.168.4.1/creds` from a connected device to retrieve all captured credentials as plain text. |
| Clear Credentials | Wipe the credential log from Settings → Portal. |

---

## Hardware Requirements

> ⚠️ **This firmware is written specifically for the ILI9488 display driver at 320×480 portrait orientation. It will not work correctly on any other display without manual reconfiguration of `TFT_eSPI`'s `User_Setup.h` and all layout constants in `main.cpp`. Do not attempt to flash this on a different display and ask for support — it requires understanding of the TFT_eSPI library and the display driver you are targeting.**

| Component | Specification |
|---|---|
| MCU | ESP32 (ESP32-WROOM-32 or equivalent) |
| Display | 3.5" SPI TFT, **ILI9488 driver**, 320×480 resolution |
| TFT Library | TFT_eSPI (pre-configured ZIP included in repo) |
| Input | 1× push button (active LOW) |

---

## Wiring

### TFT Display (ILI9488 SPI)

| TFT Pin | ESP32 Pin | Notes |
|---|---|---|
| VCC | 3V3 | — |
| GND | GND | — |
| SCK / CLK | GPIO 18 | VSPI clock |
| MOSI / SDA | GPIO 23 | VSPI data out |
| MISO | GPIO 19 | Optional — only if your display module requires it |
| CS | GPIO 5 | Chip select |
| DC / RS | GPIO 2 | Data/command |
| RST | GPIO 4 | Or tie to 3V3 for auto-reset |
| LED / BL | 3V3 | Via current-limiting resistor or transistor |

### Button

| Function | GPIO | Configuration |
|---|---|---|
| Single input button | GPIO 12 | `INPUT_PULLUP` — active LOW |

> The entire UI operates from a single button. A **short press** scrolls to the next item. A **long press** (≥520 ms) selects the current item or confirms an action.

---

## Software & Libraries

The firmware is a single Arduino sketch: `main.cpp`.

Install all libraries through the Arduino IDE Library Manager or PlatformIO before building.

| Library | Source |
|---|---|
| `TFT_eSPI` | Included as `TFT_eSPI-2.5.43.zip` — **use this version, pre-configured for ILI9488** |
| `NimBLE-Arduino` | Arduino Library Manager — search `NimBLE-Arduino` by h2zero |
| `AsyncTCP` | Arduino Library Manager — search `AsyncTCP` by me-no-dev |
| `ESPAsyncWebServer` | Arduino Library Manager — search `ESPAsyncWebServer` by me-no-dev |
| `WiFi` | Built into ESP32 Arduino core |
| `LittleFS` | Built into ESP32 Arduino core (v2+) — **do not install the separate legacy `LittleFS_esp32` package** |
| `DNSServer` | Built into ESP32 Arduino core |
| `Preferences` | Built into ESP32 Arduino core |

---

## TFT_eSPI Configuration

The repository includes `TFT_eSPI-2.5.43.zip` — a version of the library with `User_Setup.h` already configured for the ILI9488 at the pin assignments listed in the wiring table above.

**Do not install TFT_eSPI from the Library Manager if you intend to use this ZIP.** They will conflict.

### Installation Steps

1. In Arduino IDE, go to **Sketch → Include Library → Add .ZIP Library**.
2. Select `TFT_eSPI-2.5.43.zip` from this repository.
3. Do not modify `User_Setup.h` unless you are changing pin assignments for your specific board. If you do modify it, ensure the following are set:

```cpp
#define ILI9488_DRIVER
#define TFT_WIDTH  320
#define TFT_HEIGHT 480
```

Along with your SPI pin definitions matching the wiring table above.

---

## Building & Flashing

### Arduino IDE

1. Clone this repository or download as ZIP and extract.
2. Open `main.cpp` in Arduino IDE (rename to `main.ino` if the IDE requires it).
3. In **Tools → Board**, select **ESP32 Dev Module** (or your specific ESP32 board variant).
4. In **Tools → Partition Scheme**, select **Huge APP (3MB No OTA / 1MB SPIFFS)**.

   > ⚠️ **This step is mandatory.** The firmware will not fit in the default partition scheme. If you see a "Sketch too large" error, you have not changed the partition scheme.

5. Install all required libraries as listed above.
6. Select the correct COM port under **Tools → Port**.
7. Click **Upload**.

### PlatformIO

Add the following to your `platformio.ini`:

```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
board_build.partitions = huge_app.csv
lib_deps =
    h2zero/NimBLE-Arduino
    me-no-dev/AsyncTCP
    me-no-dev/ESPAsyncWebServer
```

---

## LittleFS — Uploading the Portal Page

The Evil Portal feature requires `index.html` to be flashed to the ESP32's LittleFS filesystem. Without this step, the captive portal will return a 404 error.

The `index.html` file is located in the `/data` folder of this repository.

### Method 1 — Arduino IDE (recommended for beginners)

1. Install the **ESP32 LittleFS Data Upload** plugin for Arduino IDE:
   - Download from: https://github.com/lorol/arduino-esp32littlefs-plugin/releases
   - Place the `.jar` file in your Arduino `tools/` folder (create it if it does not exist).
   - Restart Arduino IDE.
2. Ensure the `/data` folder is in the same directory as your sketch file.
3. In Arduino IDE, go to **Tools → ESP32 LittleFS Data Upload**.
4. The plugin will compile and flash the contents of `/data` to the LittleFS partition.

> ⚠️ You must select the same partition scheme (**Huge APP**) and the same COM port as when you flashed the firmware. The board must not be running the Serial Monitor during the upload.

### Method 2 — PlatformIO

1. Ensure the `/data` folder is present in your project root.
2. Run the following command:

```bash
pio run --target uploadfs
```

This will automatically build and flash the filesystem image.

### Verifying the Upload

After uploading both the firmware and the filesystem:

1. Open the Evil Portal from the main menu and start it.
2. Connect a device to the `Free_Public_WiFi` access point.
3. Navigate to any HTTP address — you should be redirected to the login page served by the ESP32.
4. If you see a blank page or a 404 error, the LittleFS upload did not complete successfully. Repeat the upload steps.

---

## Controls

The device uses a single push button on GPIO 12.

| Action | Result |
|---|---|
| Short press | Scroll to next item in current menu |
| Long press (≥520 ms) | Select / confirm / enter submenu / stop active function |

### Serial Debug Interface

With a Serial monitor open at **115200 baud**, the following characters can be sent to navigate without a physical button:

| Character | Action |
|---|---|
| `d` | Short press (scroll) |
| `e` | Long press (select) |
| `0` | Back / exit current screen |

---


---

## License

MIT License. See `LICENSE` for full terms.

This project is provided as-is for educational and research purposes. The author is not responsible for any illegal use of this software.
