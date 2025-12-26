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
​
