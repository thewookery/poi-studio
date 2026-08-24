/**
 * ============================================================================
 * OPEN PIXEL POI - PURE BLE MASTER NUNCHUK CONTROLLER (For Golden Poi Firmware)
 * ============================================================================
 * Connects directly to OpenPixelPoi over Bluetooth Low Energy (Nordic UART)
 * and transmits real-time Wii Nunchuk commands with zero changes to Poi firmware.
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>

#define NUNCHUK_ADDR 0x52

// Nordic UART Service UUIDs matching OpenPixelPoi Golden Firmware
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

#define START_BYTE 0xD0
#define END_BYTE   0xD1

// Comm Codes
#define CC_SET_BRIGHTNESS 0x02
#define CC_SET_PATTERN_SLOT 0x05
#define CC_SET_BANK 0x07
#define CC_SET_PALETTE_FX 0x15
#define CC_SET_MOTION_FX 0x18

struct I2CPair {
  uint8_t sda;
  uint8_t scl;
  const char* name;
};

const I2CPair I2C_PAIRS[] = {
  { 4, 5, "D2 (SDA) / D3 (SCL)" },
  { 6, 7, "D4 (SDA) / D5 (SCL)" }
};

int activeI2cIndex = 0;
bool isNunchukEncrypted = false;
bool isNunchukI2cOk = false;

// State Variables
uint8_t currentSlot = 0;
uint8_t currentBank = 0;
uint8_t currentPalette = 0;
uint8_t currentMotion = 0;
bool isStrobeActive = false;

// Nunchuk Live Telemetry
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
unsigned long lastI2cRetry = 0;
unsigned long lastScanTime = 0;

// BLE Multi-Poi Clients
#define MAX_BLE_POIS 2
struct BlePoiConnection {
  BLEAdvertisedDevice* advDevice = nullptr;
  BLEClient* client = nullptr;
  BLERemoteCharacteristic* rxChar = nullptr;
  bool connected = false;
  std::string address = "";
};

BlePoiConnection pois[MAX_BLE_POIS];
int connectedPoiCount = 0;
BLEScan* pBLEScan = nullptr;
bool doScan = true;

// Send Command packet to all connected BLE Pois
void sendBleCommand(uint8_t cmdCode, uint8_t val) {
  uint8_t pkt[4] = { START_BYTE, cmdCode, val, END_BYTE };
  
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (pois[i].connected && pois[i].rxChar != nullptr) {
      pois[i].rxChar->writeValue(pkt, 4, false); // write without response for instant sub-millisecond latency
    }
  }
}

// Connect to a discovered Poi device
bool connectToPoi(int slotIdx, BLEAdvertisedDevice advertisedDevice) {
  Serial.printf("[BLE] Connecting to Poi %d: %s (%s)...\n", slotIdx + 1, advertisedDevice.getName().c_str(), advertisedDevice.getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(&advertisedDevice)) {
    Serial.println("[BLE] Connection failed!");
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("[BLE] Failed to find Nordic UART service!");
    pClient->disconnect();
    return false;
  }

  BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteChar == nullptr) {
    Serial.println("[BLE] Failed to find RX characteristic!");
    pClient->disconnect();
    return false;
  }

  pois[slotIdx].client = pClient;
  pois[slotIdx].rxChar = pRemoteChar;
  pois[slotIdx].connected = true;
  pois[slotIdx].address = advertisedDevice.getAddress().toString();
  connectedPoiCount++;

  Serial.printf("✅ [BLE] Poi %d Connected & Locked! Total connected: %d\n", slotIdx + 1, connectedPoiCount);
  return true;
}

// Scan Callback to discover OpenPixelPoi devices
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = String(advertisedDevice.getName().c_str());
    
    if (devName.indexOf("OpenPixelPoi") >= 0 || devName.indexOf("Pixel Poi") >= 0 || advertisedDevice.isAdvertisingService(serviceUUID)) {
      std::string addr = advertisedDevice.getAddress().toString();
      
      // Check if already connected to this address
      for (int i = 0; i < MAX_BLE_POIS; i++) {
        if (pois[i].connected && pois[i].address == addr) {
          return; // already connected
        }
      }

      // Connect to available slot
      for (int i = 0; i < MAX_BLE_POIS; i++) {
        if (!pois[i].connected) {
          pBLEScan->stop();
          connectToPoi(i, advertisedDevice);
          break;
        }
      }
    }
  }
};

bool tryInitNunchukOnPins(uint8_t sda, uint8_t scl) {
  Wire.end();
  delay(10);
  
  pinMode(sda, INPUT_PULLUP);
  pinMode(scl, INPUT_PULLUP);
  Wire.begin(sda, scl, 50000);
  delay(20);

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

  // 1. Joystick X: Slot change (Left / Right flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyX < 50) {
      currentSlot = (currentSlot > 0) ? (currentSlot - 1) : 9;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      Serial.printf("[NUNCHUK] Switched to Slot %d\n", currentSlot + 1);
      lastJoyFlickTime = now;
    } else if (joyX > 200) {
      currentSlot = (currentSlot + 1) % 10;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      Serial.printf("[NUNCHUK] Switched to Slot %d\n", currentSlot + 1);
      lastJoyFlickTime = now;
    }
  }

  // 2. Joystick Y: Bank change (Up / Down flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyY > 200) {
      currentBank = (currentBank + 1) % 5;
      sendBleCommand(CC_SET_BANK, currentBank);
      Serial.printf("[NUNCHUK] Switched to Bank %d\n", currentBank + 1);
      lastJoyFlickTime = now;
    } else if (joyY < 50) {
      currentBank = (currentBank > 0) ? (currentBank - 1) : 4;
      sendBleCommand(CC_SET_BANK, currentBank);
      Serial.printf("[NUNCHUK] Switched to Bank %d\n", currentBank + 1);
      lastJoyFlickTime = now;
    }
  }

  // 3. C Button: Step Palette
  if (btnC && !lastBtnC) {
    btnCHoldStart = now;
  } else if (!btnC && lastBtnC) {
    unsigned long pressDur = now - btnCHoldStart;
    if (pressDur < 1000) {
      currentPalette = (currentPalette + 1) % 33;
      sendBleCommand(CC_SET_PALETTE_FX, currentPalette);
      Serial.printf("[NUNCHUK] Switched to Palette %d\n", currentPalette);
    }
  } else if (btnC && (now - btnCHoldStart > 1200) && btnCHoldStart != 0) {
    currentPalette = 0;
    sendBleCommand(CC_SET_PALETTE_FX, 0);
    Serial.println("[NUNCHUK] Reset Palette to Blank");
    btnCHoldStart = 0;
  }

  // 4. Z Trigger: Instant Strobe Drop on hold
  if (btnZ != isStrobeActive) {
    isStrobeActive = btnZ;
    sendBleCommand(CC_SET_MOTION_FX, isStrobeActive ? 15 : 0);
    Serial.printf("[NUNCHUK] Strobe Blast: %s\n", isStrobeActive ? "ON" : "OFF");
  }

  lastJoyX = joyX;
  lastJoyY = joyY;
  lastBtnC = btnC;
  lastBtnZ = btnZ;
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("=================================================");
  Serial.println("Open Pixel Poi - Pure BLE Master Nunchuk Bridge");
  Serial.println("=================================================");

  autoScanNunchuk();

  BLEDevice::init("OpenPixelNunchukMaster");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("🎮 Scanning for OpenPixelPoi over Bluetooth...");
}

void loop() {
  unsigned long now = millis();

  // Check connection status & scan if any slot is disconnected
  int activeConnections = 0;
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (pois[i].connected) {
      if (!pois[i].client->isConnected()) {
        Serial.printf("[BLE] Poi %d disconnected.\n", i + 1);
        pois[i].connected = false;
        pois[i].rxChar = nullptr;
      } else {
        activeConnections++;
      }
    }
  }
  connectedPoiCount = activeConnections;

  if (connectedPoiCount < MAX_BLE_POIS && (now - lastScanTime > 3000)) {
    lastScanTime = now;
    pBLEScan->start(2, false);
  }

  readNunchukAndProcess();
  delay(15);
}
