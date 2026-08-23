/**
 * ============================================================================
 * OPEN PIXEL POI - MASTER NUNCHUK BRIDGE (v2.3 Universal ESP-NOW OTA Gateway)
 * ============================================================================
 * Features:
 *   - Auto-scans I2C Pins: D2/D3 (GPIO 4/5) AND D4/D5 (GPIO 6/7)
 *   - Auto-detects OEM Nintendo & 3rd-Party Clone Nunchuks
 *   - ESP-NOW Over-The-Air Pattern Upload Relay (LittleFS Direct Burning)
 *   - Live Bluetooth & Wi-Fi Telemetry Stream
 *   - Sub-1ms ESP-NOW Broadcast on Channel 1
 */

#include <esp_wifi.h>
#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define NUNCHUK_ADDR 0x52
#define ESPNOW_PACKET_MAGIC 0xA5
#define ESPNOW_DATA_MAGIC   0xA6

// Nordic UART Service UUIDs
#define NORDIC_UART_SERVICE "6e400001-b5a3-f393-e0a9-e50e24dcca9e"
#define NORDIC_UART_RX_CHAR "6e400002-b5a3-f393-e0a9-e50e24dcca9e"
#define NORDIC_UART_TX_CHAR "6e400003-b5a3-f393-e0a9-e50e24dcca9e"

#define START_BYTE 0xD0
#define END_BYTE   0xD1
#define TELEMETRY_BYTE 0xFE

// Pin configurations to auto-scan
struct I2CPair {
  uint8_t sda;
  uint8_t scl;
  const char* name;
};

const I2CPair I2C_PAIRS[] = {
  { 4, 5, "D2 (SDA) / D3 (SCL)" }, // GPIO 4 / GPIO 5
  { 6, 7, "D4 (SDA) / D5 (SCL)" }  // GPIO 6 / GPIO 7
};

int activeI2cIndex = 0;
bool isNunchukEncrypted = false;
bool isNunchukI2cOk = false;

enum EspNowCommand {
  CMD_NONE = 0,
  CMD_SET_PATTERN = 1,
  CMD_SET_BANK = 2,
  CMD_SET_PALETTE = 3,
  CMD_SET_MOTION_FX = 4,
  CMD_SET_SPEED = 5,
  CMD_STROBE_BLAST = 6,
  CMD_SET_BRIGHTNESS = 7,
  CMD_TILT_MODULATION = 8,
  CMD_START_PATTERN_UPLOAD = 0x20,
  CMD_PATTERN_DATA_CHUNK = 0x21,
  CMD_END_PATTERN_UPLOAD = 0x22
};

typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t cmd;
  uint8_t val1;
  uint8_t val2;
  int16_t tiltX;
  int16_t tiltY;
  int16_t tiltZ;
} EspNowPacket;

typedef struct __attribute__((packed)) {
  uint8_t magic;
  uint8_t cmd;
  uint16_t chunkSeq;
  uint8_t dataLen;
  uint8_t data[200];
} EspNowDataPacket;

static uint8_t broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// State Variables
uint8_t currentSlot = 0;
uint8_t currentBank = 0;
uint8_t currentPalette = 0;
uint8_t currentMotion = 0;
uint8_t currentBrightness = 200;
bool isStrobeActive = false;

// OTA Upload State
bool isOtaUploading = false;
uint16_t otaChunkSeq = 0;

uint8_t liveJoyX = 128;
uint8_t liveJoyY = 128;
uint8_t lastJoyX = 128;
uint8_t lastJoyY = 128;
bool liveBtnC = false;
bool liveBtnZ = false;
bool lastBtnC = false;
bool lastBtnZ = false;
int16_t liveAccelX = 512;
int16_t liveAccelY = 512;
int16_t liveAccelZ = 512;

unsigned long btnCHoldStart = 0;
unsigned long lastJoyFlickTime = 0;
unsigned long lastTelemetryNotify = 0;
unsigned long lastI2cRetry = 0;

BLECharacteristic *pGlobalTxChar = nullptr;
bool isBleClientConnected = false;

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

void forwardPatternData(const uint8_t *data, size_t len) {
  size_t offset = 0;
  while (offset < len) {
    size_t chunk = min((size_t)200, len - offset);
    EspNowDataPacket dPkt;
    dPkt.magic = ESPNOW_DATA_MAGIC;
    dPkt.cmd = CMD_PATTERN_DATA_CHUNK;
    dPkt.chunkSeq = otaChunkSeq++;
    dPkt.dataLen = (uint8_t)chunk;
    memcpy(dPkt.data, data + offset, chunk);
    esp_now_send(broadcastMac, (uint8_t *)&dPkt, sizeof(EspNowDataPacket));
    offset += chunk;
    delay(2); // Safe transmission pacing
  }
}

class BridgeServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    isBleClientConnected = true;
    Serial.println("BLE Client Connected to Bridge!");
  }
  void onDisconnect(BLEServer* pServer) {
    isBleClientConnected = false;
    Serial.println("BLE Client Disconnected from Bridge!");
    BLEDevice::startAdvertising();
  }
};

class BridgeBleCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    size_t len = rxValue.length();
    if (len == 0) return;
    const uint8_t *data = (const uint8_t *)rxValue.data();

    // 1. Check for Pattern Upload Header / Chunks
    if (data[0] == START_BYTE && len >= 5 && data[1] == 0x04) {
      // Chunk 0: Start of new Pattern Upload
      uint8_t height = data[2];
      uint16_t width = ((uint16_t)data[3] << 8) | data[4];
      uint8_t targetSlot = (currentBank * 10) + currentSlot;

      isOtaUploading = true;
      otaChunkSeq = 0;
      sendEspNowPacket(CMD_START_PATTERN_UPLOAD, targetSlot, height, width);
      delay(10);

      // Forward RGB payload in chunk 0 (bytes 5..end)
      if (len > 5) {
        size_t dataLen = len - 5;
        if (data[len - 1] == END_BYTE) {
          dataLen--;
        }
        if (dataLen > 0) {
          forwardPatternData(data + 5, dataLen);
        }
      }

      if (data[len - 1] == END_BYTE || len < 509) {
        isOtaUploading = false;
        delay(15);
        sendEspNowPacket(CMD_END_PATTERN_UPLOAD, targetSlot);
        Serial.printf("[Bridge] Completed single-packet OTA pattern upload to slot %d!\n", targetSlot);
      }
      return;
    }

    // Subsequent Chunks during Pattern Upload
    if (isOtaUploading) {
      size_t dataLen = len;
      bool isFinal = false;
      if (data[len - 1] == END_BYTE) {
        dataLen--;
        isFinal = true;
      }
      if (dataLen > 0) {
        forwardPatternData(data, dataLen);
      }
      if (isFinal || len < 509) {
        isOtaUploading = false;
        uint8_t targetSlot = (currentBank * 10) + currentSlot;
        delay(20);
        sendEspNowPacket(CMD_END_PATTERN_UPLOAD, targetSlot);
        Serial.printf("[Bridge] Completed multi-chunk OTA pattern upload to slot %d!\n", targetSlot);
      }
      return;
    }

    // 2. Standard Framed / Direct Commands
    uint8_t cmdCode = 0;
    uint8_t val = 0;

    if (data[0] == START_BYTE && data[len - 1] == END_BYTE) {
      cmdCode = data[1];
      val = (len >= 4) ? data[2] : 0;
    } else {
      cmdCode = data[0];
      val = (len >= 2) ? data[1] : 0;
    }

    if (cmdCode == 0x04 || cmdCode == 0x05) {
      currentSlot = val % 10;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
    } else if (cmdCode == 0x07 || cmdCode == 0x08) {
      currentBank = val % 5;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
    } else if (cmdCode == 0x02 || cmdCode == 0x10) {
      currentBrightness = val;
      sendEspNowPacket(CMD_SET_BRIGHTNESS, val);
    } else if (cmdCode == 0x15) {
      currentPalette = val % 33;
      sendEspNowPacket(CMD_SET_PALETTE, currentPalette);
    } else if (cmdCode == 0x18) {
      currentMotion = val % 17;
      sendEspNowPacket(CMD_SET_MOTION_FX, currentMotion);
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
    body { background:#0a0d14; color:#fff; font-family:sans-serif; text-align:center; padding:12px; margin:0; }
    h2 { color:#00ff88; margin:4px 0; }
    .card { background:#141824; border:1px solid rgba(255,255,255,0.1); border-radius:10px; padding:12px; margin:8px auto; max-width:340px; }
    .btn-grid { display:grid; grid-template-columns:repeat(5, 1fr); gap:6px; margin-top:6px; }
    button { background:#1f2438; color:#00d2ff; border:1px solid #00d2ff; border-radius:6px; padding:10px 4px; font-weight:bold; font-size:14px; cursor:pointer; }
    button:active { background:#00d2ff; color:#000; }
    .btn-strobe { width:100%; background:#ff0055; color:#fff; border:none; padding:14px; font-size:16px; border-radius:8px; font-weight:900; margin-top:6px; }
    .btn-strobe:active { background:#fff; color:#ff0055; }
    .status-pill { display:inline-block; padding:3px 8px; border-radius:12px; font-size:11px; font-weight:bold; }
    .stick-box { width:100px; height:100px; background:#070a11; border:2px solid #00ff88; border-radius:50%; margin:8px auto; position:relative; }
    .stick-dot { width:14px; height:14px; background:#00ff88; border-radius:50%; position:absolute; top:43px; left:43px; transform:translate(0,0); }
  </style>
</head>
<body>
  <h2>⚡ OPEN PIXEL BRIDGE</h2>
  <div style="font-size:12px; color:#888;">Direct ESP-NOW Mesh (Channel 1)</div>

  <div class="card" style="border-color:#00ff88;">
    <div style="display:flex; justify-content:space-between; align-items:center;">
      <b style="color:#00ff88;">🎮 Nunchuk Diagnostics</b>
      <span id="nunchuk-badge" class="status-pill" style="background:rgba(255,77,77,0.2); color:#ff4d4d;">Scanning...</span>
    </div>
    <div class="stick-box">
      <div id="stick-dot" class="stick-dot"></div>
    </div>
    <div style="display:flex; justify-content:space-around; font-size:12px; font-family:monospace;">
      <span>X: <b id="val-x">128</b> | Y: <b id="val-y">128</b></span>
      <span>Z: <b id="val-z">0</b> | C: <b id="val-c">0</b></span>
    </div>
    <div id="pin-info" style="font-size:11px; color:#aaa; margin-top:6px;">Scanning D2/D3 & D4/D5...</div>
  </div>

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

    setInterval(async () => {
      try {
        const res = await fetch('/api?act=telemetry');
        const data = await res.json();
        const badge = document.getElementById('nunchuk-badge');
        if (data.ok) {
          badge.innerText = '🟢 I2C OK (' + data.pins + ')';
          badge.style.background = 'rgba(0,255,136,0.2)';
          badge.style.color = '#00ff88';
        } else {
          badge.innerText = '🔴 I2C Scanning...';
          badge.style.background = 'rgba(255,77,77,0.2)';
          badge.style.color = '#ff4d4d';
        }
        document.getElementById('val-x').innerText = data.x;
        document.getElementById('val-y').innerText = data.y;
        document.getElementById('val-z').innerText = data.z ? 'ON' : '0';
        document.getElementById('val-c').innerText = data.c ? 'ON' : '0';
        document.getElementById('pin-info').innerText = 'Active Pins: ' + data.pins;

        const dx = ((data.x - 128) / 128) * 35;
        const dy = -((data.y - 128) / 128) * 35;
        document.getElementById('stick-dot').style.transform = `translate(${dx}px, ${dy}px)`;
      } catch(e) {}
    }, 120);
  </script>
</body>
</html>
)rawliteral";

void handleRoot() {
  server.send(200, "text/html", INDEX_HTML);
}

void handleApi() {
  if (server.hasArg("act")) {
    String act = server.arg("act");
    if (act == "telemetry") {
      String json = "{\"ok\":" + String(isNunchukI2cOk ? "true" : "false") +
                    ",\"x\":" + String(liveJoyX) +
                    ",\"y\":" + String(liveJoyY) +
                    ",\"z\":" + String(liveBtnZ ? 1 : 0) +
                    ",\"c\":" + String(liveBtnC ? 1 : 0) +
                    ",\"ax\":" + String(liveAccelX) +
                    ",\"ay\":" + String(liveAccelY) +
                    ",\"az\":" + String(liveAccelZ) +
                    ",\"pins\":\"" + String(I2C_PAIRS[activeI2cIndex].name) + "\"}";
      server.send(200, "application/json", json);
      return;
    }

    if (server.hasArg("val")) {
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
  }
  server.send(200, "application/json", "{\"status\":\"ok\"}");
}

bool tryInitNunchukOnPins(uint8_t sda, uint8_t scl) {
  Wire.end();
  delay(10);
  
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  Wire.begin(sda, scl, 50000);
  delay(20);

  // 1. Try Modern Unencrypted Init (0xF0 -> 0x55, 0xFB -> 0x00)
  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0xF0);
  Wire.write(0x55);
  uint8_t err1 = Wire.endTransmission();
  delay(5);

  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0xFB);
  Wire.write(0x00);
  uint8_t err2 = Wire.endTransmission();
  delay(5);

  if (err1 == 0 && err2 == 0) {
    isNunchukEncrypted = false;
    Serial.println("  -> Handshake Success: Modern Unencrypted Nunchuk");
    return true;
  }

  // 2. Try Legacy Encrypted Init (0x40 -> 0x00)
  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0x40);
  Wire.write(0x00);
  uint8_t err3 = Wire.endTransmission();
  delay(5);

  if (err3 == 0) {
    isNunchukEncrypted = true;
    Serial.println("  -> Handshake Success: Legacy Encrypted Nunchuk");
    return true;
  }

  return false;
}

void autoScanNunchuk() {
  for (int i = 0; i < 2; i++) {
    Serial.printf("Scanning Nunchuk on %s (SDA=%d, SCL=%d)...\n", I2C_PAIRS[i].name, I2C_PAIRS[i].sda, I2C_PAIRS[i].scl);
    if (tryInitNunchukOnPins(I2C_PAIRS[i].sda, I2C_PAIRS[i].scl)) {
      activeI2cIndex = i;
      isNunchukI2cOk = true;
      Serial.printf("✅ Nunchuk LOCKED on %s!\n", I2C_PAIRS[i].name);
      return;
    }
  }
  isNunchukI2cOk = false;
  Serial.println("❌ Nunchuk not responding on D2/D3 or D4/D5.");
}

void setupEspNowAndWiFi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP("OpenPixelBridge", "openpixelbridge", 1);

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Bridge Init Failed!");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 1;
  peerInfo.ifidx = WIFI_IF_AP;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  server.on("/", handleRoot);
  server.on("/api", handleApi);
  server.begin();

  Serial.println("ESP-NOW Mesh & Wi-Fi Web Dashboard Active at 192.168.4.1!");
}

void setupBleGateway() {
  BLEDevice::init("OpenPixelBridge-Nunchuk");
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BridgeServerCallbacks());

  BLEService *pService = pServer->createService(NORDIC_UART_SERVICE);

  BLECharacteristic *pRxChar = pService->createCharacteristic(
    NORDIC_UART_RX_CHAR,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pRxChar->setCallbacks(new BridgeBleCallbacks());

  pGlobalTxChar = pService->createCharacteristic(
    NORDIC_UART_TX_CHAR,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pGlobalTxChar->addDescriptor(new BLE2902());

  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(NORDIC_UART_SERVICE);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Nordic UART Web Bluetooth Active!");
}

inline uint8_t decodeNunchukByte(uint8_t b) {
  return isNunchukEncrypted ? ((b ^ 0x17) + 0x17) : b;
}

void readNunchukAndProcess() {
  unsigned long now = millis();

  if (!isNunchukI2cOk && (now - lastI2cRetry > 1500)) {
    lastI2cRetry = now;
    autoScanNunchuk();
    if (!isNunchukI2cOk) return;
  }

  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0x00);
  if (Wire.endTransmission() != 0) {
    isNunchukI2cOk = false;
    return;
  }
  delayMicroseconds(300);

  Wire.requestFrom(NUNCHUK_ADDR, 6);
  if (Wire.available() < 6) {
    isNunchukI2cOk = false;
    return;
  }

  isNunchukI2cOk = true;
  uint8_t raw[6];
  for (int i = 0; i < 6; i++) {
    raw[i] = decodeNunchukByte(Wire.read());
  }

  uint8_t joyX = raw[0];
  uint8_t joyY = raw[1];
  int16_t accelX = (raw[2] << 2) | ((raw[5] >> 2) & 0x03);
  int16_t accelY = (raw[3] << 2) | ((raw[5] >> 4) & 0x03);
  int16_t accelZ = (raw[4] << 2) | ((raw[5] >> 6) & 0x03);
  bool btnZ = !((raw[5] >> 0) & 0x01);
  bool btnC = !((raw[5] >> 1) & 0x01);

  if (joyX == 0 && joyY == 0 && raw[2] == 0) {
    isNunchukI2cOk = false;
    return;
  }

  liveJoyX = joyX;
  liveJoyY = joyY;
  liveBtnZ = btnZ;
  liveBtnC = btnC;
  liveAccelX = accelX;
  liveAccelY = accelY;
  liveAccelZ = accelZ;

  if (isBleClientConnected && pGlobalTxChar != nullptr && (now - lastTelemetryNotify > 60)) {
    lastTelemetryNotify = now;
    uint8_t telPkt[10];
    telPkt[0] = START_BYTE;
    telPkt[1] = TELEMETRY_BYTE;
    telPkt[2] = joyX;
    telPkt[3] = joyY;
    telPkt[4] = btnZ ? 1 : 0;
    telPkt[5] = btnC ? 1 : 0;
    telPkt[6] = (uint8_t)(accelX >> 2);
    telPkt[7] = (uint8_t)(accelY >> 2);
    telPkt[8] = isNunchukI2cOk ? 1 : 0;
    telPkt[9] = END_BYTE;
    pGlobalTxChar->setValue(telPkt, 10);
    pGlobalTxChar->notify();
  }

  // 1. Joystick X: Slot change (Left / Right flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyX < 50) {
      currentSlot = (currentSlot > 0) ? (currentSlot - 1) : 9;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      lastJoyFlickTime = now;
    } else if (joyX > 200) {
      currentSlot = (currentSlot + 1) % 10;
      sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      lastJoyFlickTime = now;
    }
  }

  // 2. Joystick Y: Bank change (Up / Down flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyY > 200) {
      currentBank = (currentBank + 1) % 5;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
      lastJoyFlickTime = now;
    } else if (joyY < 50) {
      currentBank = (currentBank > 0) ? (currentBank - 1) : 4;
      sendEspNowPacket(CMD_SET_BANK, currentBank);
      lastJoyFlickTime = now;
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
    }
  } else if (btnC && (now - btnCHoldStart > 1200) && btnCHoldStart != 0) {
    currentPalette = 0;
    sendEspNowPacket(CMD_SET_PALETTE, 0);
    btnCHoldStart = 0;
  }

  // 4. Z Trigger: Instant Strobe Drop on hold
  if (btnZ != isStrobeActive) {
    isStrobeActive = btnZ;
    sendEspNowPacket(CMD_STROBE_BLAST, isStrobeActive ? 1 : 0);
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
  Serial.println("Starting Open Pixel Master Nunchuk Bridge v2.3 (ESP-NOW OTA Gateway)...");

  autoScanNunchuk();
  setupEspNowAndWiFi();
  setupBleGateway();

  Serial.println("🎉 Bridge Ready! ESP-NOW OTA Gateway + Bluetooth + Wi-Fi Telemetry Active.");
}

void loop() {
  server.handleClient();
  readNunchukAndProcess();
  delay(10);
}
