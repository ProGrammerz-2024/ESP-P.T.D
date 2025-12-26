esp ptd is an ESP32‑based portable toolkit that combines Wi‑Fi and BLE utilities in a single, touchscreen‑driven interface. It is designed as a compact “pocket tool” for exploring, testing, and demonstrating common wireless behaviors on 2.4 GHz networks.

Features
Interactive TFT UI with buttons for navigating modes and viewing live status updates.

Wi‑Fi scanner for listing nearby access points with SSID, channel, and other basic details.

Experimental deauthentication mode targeting selected 2.4 GHz access points for lab and educational use only.

BLE sniffer that displays live BLE advertising packets and device addresses.

BLE “beacon spam” mode that rapidly transmits custom BLE advertisement frames.

Hardware and software
Built for ESP32 development boards with SPI‑driven TFT displays.

Uses a bundled copy of TFT_eSPI for display handling plus NimBLE for BLE functionality.

Structured as a single Arduino sketch with a small set of local libraries so it can be built directly in the Arduino IDE.

Libraries used:

 <Arduino.h>
 <TFT_eSPI.h>
 <WiFi.h>
 "esp_wifi.h"
 "esp_wifi_types.h"
 <NimBLEDevice.h>
 <DNSServer.h>
 <AsyncTCP.h>
 <ESPAsyncWebServer.h>
 <LittleFS.h>
 <BleKeyboard.h>

 Usage: Everything is given, just flash!

Hardware(IMPORTANT)- Library is specific to il9488 3.5inch screen, don't mess. And esp32.

All credit goes to justcallmekoko, for inspiration, and yeah for making, I did it ;).

Connections:

TFT VCC → ESP32 3V3

TFT GND → ESP32 GND

TFT SCK / CLK → ESP32 GPIO 18 (VSPI SCK)

TFT MOSI / SDA → ESP32 GPIO 23 (VSPI MOSI)

TFT MISO → not used (or ESP32 GPIO 19 if your display needs it)

TFT CS → ESP32 GPIO 5

TFT DC / RS → ESP32 GPIO 2

TFT RST → ESP32 GPIO 4 (or 3V3 if you use auto‑reset)

TFT LED / BL → 3V3 (through a resistor or via a transistor if backlight draws a lot)


​
