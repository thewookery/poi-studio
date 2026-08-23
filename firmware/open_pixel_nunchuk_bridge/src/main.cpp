/**
 * ============================================================================
 * OPEN PIXEL POI - MASTER NUNCHUK BRIDGE TRANSMITTER (v2.0)
 * ============================================================================
 * Hardware: Seeed Studio XIAO ESP32-C3
 * Pins:
 *   - I2C SDA: D2 (GPIO 4)
 *   - I2C SCL: D3 (GPIO 5)
 *   - 3V3: Power to Nunchuk VCC
 *   - GND: Ground to Nunchuk GND
 * 
 * Wireless Features:
 *   1. Direct ESP-NOW 2.4GHz broadcast to all Open Pixel Poi props (<1ms latency)
 *   2. Nordic UART Service (NUS) Web Bluetooth for 1-tap Open POI Studio pairing
 *   3. Standalone Wi-Fi Access Point ("OpenPixelBridge") with built-in Web Control UI
 *   4. Standalone Wii Nunchuk Joystick & Strobe Trigger Control
 */

#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define NUNCHUK_ADDR 0x52

#define ESPNOW_PACKET_MAGIC 0xA5

// Official Nordic UART Service UUIDs (matches Open POI Studio Web Bluetooth)
#define NORDIC_UART_SERVICE "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NORDIC_UART_RX_CHAR "6e400002-b5a3-f393-e0a9-e50e24dcca9e" // Write from App
#define NORDIC_UART_TX_CHAR "6e400003-b5a3-f393-e0a9-e50e24dcca9e" // Notify to App

// Framing Protocol
#define START_BYTE 0xD0
#define END_BYTE   0xD1

// Open Pixel Poi Command Codes
#define CC_SET_BRIGHTNESS   0x02
#define CC_SET_SPEED        0x03
#define CC_SET_PATTERN      0x04
#define CC_SET_PATTERN_SLOT 0x05
#define CC_SET_BANK         0x07
#define CC_SET_PALETTE_FX   0x15
#define CC_SET_MOTION_FX    0x18

enum EspNowCommand {
  CMD_NONE = 0,
  CMD_SET_PATTERN = 1,     // val1 = slot (0..9)
  CMD_SET_BANK = 2,        // val1 = bank (0..4)
  CMD_SET_PALETTE = 3,     // val1 = paletteId (0..32)
  CMD_SET_MOTION_FX = 4,   // val1 = motionId (0..16)
  CMD_SET_SPEED = 5,       // val1, val2 = speedHz (uint16_t)
  CMD_STROBE_BLAST = 6,    // val1 = 1 (active) / 0 (inactive)
  CMD_SET_BRIGHTNESS = 7,  // val1 = brightness (0..255)
  CMD_TILT_MODULATION = 8  // tiltX, tiltY, tiltZ
};

typedef struct __attribute__((packed)) {
  uint8_t magic;           // 0xA5
  uint8_t cmd;             // EspNowCommand
  uint8_t val1;            // Slot / Bank / Pal / Motion / Brightness
  uint8_t val2;            // Secondary parameter
  int16_t tiltX;           // Nunchuk Accelerometer X
  int16_t tiltY;           // Nunchuk Accelerometer Y
  int16_t tiltZ;           // Nunchuk Accelerometer Z
} EspNowPacket;

static uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Global Bridge State
uint8_t currentSlot = 0;
uint8_t currentBank = 0;
uint8_t currentPalette = 0;
uint8_t currentMotion = 0;
uint8_t currentBrightness = 200;
bool isStrobeActive = false;

// Nunchuk State
uint8_t lastJoyX = 128;
uint8_t lastJoyY = 128;
bool lastBtnC = false;
bool lastBtnZ = false;
unsigned long btnCHoldStart = 0;
unsigned long lastJoyFlickTime = 0;

// Web Server for Wi-Fi Hotspot Mode
WebServer server(80);

void sendEspNowPacket(uint8_t cmd, uint8_t val1 = 0, uint8_t val2 = 0, int16_t tx = 0, int16_t ty = 0, int16_t tz = 0) {
  EspNowPacket pkt;
  pkt.magic = ESPNOW_PACKET_MAGIC;
  pkt.cmd = cmd;
  pkt.val1 = val1;
  pkt.val2 = val2;
  pkt.tiltX = tx;
  pkt.tiltY = ty;
  pkt.tiltZ = tz;
  esp_now_send(broadcastMac, (uint8_t *)&pkt, sizeof(EspNowPacket));
}

// BLE Callback: forwards framed Open POI Studio commands over ESP-NOW
class BridgeBleCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() >= 3) {
      const uint8_t *data = (const uint8_t *)rxValue.data();
      // Check framing or direct packet
      uint8_t cmdCode = 0;
      uint8_t val = 0;

      if (data[0] == START_BYTE && data[rxValue.length() - 1] == END_BYTE) {
        cmdCode = data[1];
        val = (rxValue.length() >= 4) ? data[2] : 0;
      } else {
        cmdCode = data[0];
        val = (rxValue.length() >= 2) ? data[1] : 0;
      }

      if (cmdCode == CC_SET_PATTERN || cmdCode == CC_SET_PATTERN_SLOT) {
        currentSlot = val % 10;
        sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      } else if (cmdCode == CC_SET_BANK) {
        currentBank = val % 5;
        sendEspNowPacket(CMD_SET_BANK, currentBank);
      } else if (cmdCode == CC_SET_BRIGHTNESS) {
        currentBrightness = val;
        sendEspNowPacket(CMD_SET_BRIGHTNESS, val);
      } else if (cmdCode == CC_SET_PALETTE_FX) {
        currentPalette = val % 33;
        sendEspNowPacket(CMD_SET_PALETTE, currentPalette);
      } else if (cmdCode == CC_SET_MOTION_FX) {
        currentMotion = val % 17;
        sendEspNowPacket(CMD_SET_MOTION_FX, currentMotion);
      }
    }
  }
};

// Wi-Fi Web Dashboard HTML
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no">
  <title>Open Pixel Bridge</title>
  <style>
    body { background:#0a0d14; color:#fff; font-family:sans-serif; text-align:center; padding:16px; margin:0; }
    h2 { color:#00ff88; margin-bottom:4px; }
    .card { background:#141824; border:1px solid rgba(255,255,255,0.1); border-radius:10px; padding:14px; margin:10px auto; max-width:340px; }
    .btn-grid { display:grid; grid-template-columns:repeat(5, 1fr); gap:6px; margin-top:8px; }
    button { background:#1f2438; color:#00d2ff; border:1px solid #00d2ff; border-radius:6px; padding:10px 4px; font-weight:bold; font-size:14px; cursor:pointer; }
    button:active { background:#00d2ff; color:#000; }
    .btn-strobe { width:100%; background:#ff0055; color:#fff; border:none; padding:16px; font-size:18px; border-radius:8px; font-weight:900; margin-top:8px; }
    .btn-strobe:active { background:#fff; color:#ff0055; }
    .badge { font-size:12px; color:#888; margin-top:4px; }
  </style>
</head>
<body>
  <h2>⚡ OPEN PIXEL BRIDGE</h2>
  <div class="badge">Direct ESP-NOW Wireless Mesh Active</div>

  <div class="card">
    <b>🎯 Pattern Slot (1-10)</b>
    <div class="btn-grid">
      <button onclick="sendCmd('slot', 0)">1</button>
      <button onclick="sendCmd('slot', 1)">2</button>
      <button onclick="sendCmd('slot', 2)">3</button>
      <button onclick="sendCmd('slot', 3)">4</button>
      <button onclick="sendCmd('slot', 4)">5</button>
      <button onclick="sendCmd('slot', 5)">6</button>
      <button onclick="sendCmd('slot', 6)">7</button>
      <button onclick="sendCmd('slot', 7)">8</button>
      <button onclick="sendCmd('slot', 8)">9</button>
      <button onclick="sendCmd('slot', 9)">10</button>
    </div>
  </div>

  <div class="card">
    <b>📁 Hardware Bank (1-5)</b>
    <div class="btn-grid" style="grid-template-columns:repeat(5, 1fr);">
      <button onclick="sendCmd('bank', 0)">B1</button>
      <button onclick="sendCmd('bank', 1)">B2</button>
      <button onclick="sendCmd('bank', 2)">B3</button>
      <button onclick="sendCmd('bank', 3)">B4</button>
      <button onclick="sendCmd('bank', 4)">B5</button>
    </div>
  </div>

  <div class="card">
    <b>⚡ Strobe Blinder</b>
    <button class="btn-strobe" onmousedown="sendCmd('strobe', 1)" onmouseup="sendCmd('strobe', 0)" ontouchstart="sendCmd('strobe', 1)" ontouchend="sendCmd('strobe', 0)">💥 HOLD FOR STROBE</button>
  </div>

  <script>
    function sendCmd(act, val) {
      fetch('/api?act=' + act + '&val=' + val);
    }
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleApi() {
  if (server.hasArg("act") && server.hasArg("val")) {
    String act = server.arg("act");
    int val = server.arg("val").toInt();
    if (act == "slot") {
      currentSlot = val % 10;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
    } else if (act == "bank") {
      currentBank = val % 5;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
    } else if (act == "strobe") {
      sendEspNowPacket(CMD_STROBE_BLAST, val ? 1 : 0);
    } else if (act == "pal") {
      currentPalette = val % 33;
      sendEspNowPacket(CMD_SET_PALETTE, currentPalette);
    }
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

void setupNunchuk() {
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN, 100000);
  delay(10);

  // Modern unencrypted initialization
  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0xF0);
  Wire.write(0x55);
  Wire.endTransmission();
  delay(2);

  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0xFB);
  Wire.write(0x00);
  Wire.endTransmission();
  delay(2);

  Serial.println("Wii Nunchuk I2C Initialized!");
}

void setupEspNowAndWiFi() {
  // Set Wi-Fi to AP + Station mode on Channel 1
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("OpenPixelBridge", "openpixelbridge", 1); // Channel 1, matching ESP-NOW

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Bridge Init Failed!");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  // Setup Web Server
  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.begin();

  Serial.println("ESP-NOW Mesh & Wi-Fi Web Dashboard Active at 192.168.4.1!");
}

void setupBleGateway() {
  BLEDevice::init("OpenPixelBridge-Nunchuk");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(NORDIC_UART_SERVICE);

  BLECharacteristic *pRxChar = pService->createCharacteristic(
    NORDIC_UART_RX_CHAR,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRxChar->setCallbacks(new BridgeBleCallbacks());

  BLECharacteristic *pTxChar = pService->createCharacteristic(
    NORDIC_UART_TX_CHAR,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pTxChar->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NORDIC_UART_SERVICE);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Nordic UART Web Bluetooth Active!");
}

void readNunchukAndProcess() {
  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  delayMicroseconds(250);

  Wire.requestFrom(NUNCHUK_ADDR, 6);
  if (Wire.available() < 6) return;

  uint8_t data[6];
  for (int i = 0; i < 6; i++) {
    data[i] = Wire.read();
  }

  uint8_t joyX = data[0];
  uint8_t joyY = data[1];
  int16_t accelX = (data[2] << 2) | ((data[5] >> 2) & 0x03);
  int16_t accelY = (data[3] << 2) | ((data[5] >> 4) & 0x03);
  int16_t accelZ = (data[4] << 2) | ((data[5] >> 6) & 0x03);
  bool btnZ = !((data[5] >> 0) & 0x01); // Trigger pressed = 1
  bool btnC = !((data[5] >> 1) & 0x01); // Top button pressed = 1

  unsigned long now = millis();

  // 1. Joystick X: Slot change (Left / Right flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyX < 50) { // Flick Left -> Previous Slot
      currentSlot = (currentSlot > 0) ? (currentSlot - 1) : 9;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      lastJoyFlickTime = now;
      Serial.printf("Joystick Left -> Slot %d\n", currentSlot);
    } else if (joyX > 200) { // Flick Right -> Next Slot
      currentSlot = (currentSlot + 1) % 10;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      lastJoyFlickTime = now;
      Serial.printf("Joystick Right -> Slot %d\n", currentSlot);
    }
  }

  // 2. Joystick Y: Bank change (Up / Down flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyY > 200) { // Flick Up -> Next Bank
      currentBank = (currentBank + 1) % 5;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
      lastJoyFlickTime = now;
      Serial.printf("Joystick Up -> Bank %d\n", currentBank);
    } else if (joyY < 50) { // Flick Down -> Previous Bank
      currentBank = (currentBank > 0) ? (currentBank - 1) : 4;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
      lastJoyFlickTime = now;
      Serial.printf("Joystick Down -> Bank %d\n", currentBank);
    }
  }

  // 3. C Button: Step Palette on tap, Reset to Blank on hold > 1.2s
  if (btnC && !lastBtnC) {
    btnCHoldStart = now;
  } else if (!btnC && lastBtnC) {
    unsigned long pressDur = now - btnCHoldStart;
    if (pressDur < 1000) {
      currentPalette = (currentPalette + 1) % 33;
      sendEspNowPacket(CMD_SET_PALETTE, currentPalette);
      Serial.printf("C Button Tap -> Palette %d\n", currentPalette);
    }
  } else if (btnC && (now - btnCHoldStart > 1200) && btnCHoldStart != 0) {
    currentPalette = 0; // Blank (Original RGB)
    sendEspNowPacket(CMD_SET_PALETTE, 0);
    Serial.println("C Button Hold -> Reset to Blank Palette (0)");
    btnCHoldStart = 0; // Don't re-trigger
  }

  // 4. Z Trigger: Instant Strobe Drop on hold
  if (btnZ != isStrobeActive) {
    isStrobeActive = btnZ;
    sendEspNowPacket(CMD_STROBE_BLAST, isStrobeActive ? 1 : 0);
    Serial.printf("Z Trigger -> Strobe %s\n", isStrobeActive ? "ACTIVE" : "OFF");
  }

  // 5. Accelerometer Tilt Modulation (every 50ms)
  static unsigned long lastTiltTime = 0;
  if (now - lastTiltTime > 50) {
    lastTiltTime = now;
    sendEspNowPacket(CMD_TILT_MODULATION, 0, 0, accelX, accelY, accelZ);
  }

  lastJoyX = joyX;
  lastJoyY = joyY;
  lastBtnC = btnC;
  lastBtnZ = btnZ;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("Starting Open Pixel Master Nunchuk Bridge v2.0...");

  setupNunchuk();
  setupEspNowAndWiFi();
  setupBleGateway();

  Serial.println("🎉 Bridge Ready! Bluetooth: 'OpenPixelBridge-Nunchuk' | Wi-Fi AP: 'OpenPixelBridge' (192.168.4.1)");
}

void loop() {
  server.handleClient();
  readNunchukAndProcess();
  delay(10);
}
