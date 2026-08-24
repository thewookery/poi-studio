/**
 * ============================================================================
 * OPEN PIXEL POI - PURE BLE MASTER NUNCHUK BRIDGE (v2.12 Rock-Solid Advertising)
 * ============================================================================
 * Hardware Layout:
 * - Battery: 18350 3.7V LiPo on BAT+ / BAT- pads (Back of board)
 * - Power Button: Connected across D0 (Pin 1 / GPIO 2) and D1 (Pin 2 / GPIO 3)
 *   - Short Press: Wakes up / changes mode
 *   - Long Press (1.5s): Enters Ultra-Low Power Deep Sleep (Power OFF)
 *   - Auto-Sleep: 10 minutes of inactivity
 * - Bluetooth Advertising:
 *   - Clean 31-byte advertising packet (Name: "OpenPixelBridge")
 *   - Guaranteed discovery on Chrome, Edge, Android, iOS, Mac, and Windows!
 * - Nunchuk I2C: 3V3, GND, D2 (SDA / GPIO 4), D3 (SCL / GPIO 5)
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include <BLE2902.h>
#include "esp_sleep.h"
#include "esp_adc_cal.h"

#define PIN_BUTTON_GND D0  // GPIO 2 - Software Ground
#define PIN_BUTTON_IN  D1  // GPIO 3 - Button Input (Pulled UP)
#define PIN_BATTERY_ADC 0  // GPIO 0 / A0 - Battery Voltage ADC
#define NUNCHUK_ADDR   0x52

static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");    // RX Characteristic
static BLEUUID txCharUUID("6e400003-b5a3-f393-e0a9-e50e24dcca9e");  // TX Characteristic

#define START_BYTE 0xD0
#define END_BYTE   0xD1
#define TELEMETRY_BYTE 0xFE

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

// Battery Telemetry
uint16_t liveBatteryMv = 3850;
uint8_t liveBatteryPct = 80;
bool isUsbCharging = false;
unsigned long lastBatteryCheck = 0;

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
unsigned long lastScanStartTime = 0;
unsigned long lastUserActivityTime = 0;
unsigned long pwrBtnPressStart = 0;
bool pwrBtnPressed = false;
unsigned long lastTelemetryNotify = 0;

BLECharacteristic *pBridgeTxChar = nullptr;
BLECharacteristic *pBridgeRxChar = nullptr;
bool isWebClientConnected = false;

// Dual-Poi Connection Pool
#define MAX_BLE_POIS 2
struct BlePoiSlot {
  BLEClient* client = nullptr;
  BLERemoteCharacteristic* rxChar = nullptr;
  bool connected = false;
  std::string address = "";
  std::string name = "";
};

BlePoiSlot poiSlots[MAX_BLE_POIS];
static BLEAdvertisedDevice* pendingDevice = nullptr;
static bool doConnectPending = false;
BLEScan* pBLEScan = nullptr;

// Measure 18350 Battery Millivolts & Calculate Percentage & Charging Status
void updateBatteryTelemetry() {
  unsigned long now = millis();
  if (now - lastBatteryCheck < 1000 && lastBatteryCheck != 0) return;
  lastBatteryCheck = now;

  uint32_t rawSum = 0;
  for (int i = 0; i < 16; i++) {
    rawSum += analogReadMilliVolts(PIN_BATTERY_ADC);
    delayMicroseconds(50);
  }
  uint32_t mv = (rawSum / 16) * 2;

  if (mv < 2500) mv = 3700;
  if (mv > 4350) mv = 4350;

  liveBatteryMv = (uint16_t)mv;

  if (liveBatteryMv >= 4130) {
    isUsbCharging = true;
  } else if (liveBatteryMv <= 4050) {
    isUsbCharging = false;
  }

  if (liveBatteryMv >= 4180) {
    liveBatteryPct = 100;
  } else if (liveBatteryMv <= 3300) {
    liveBatteryPct = 0;
  } else {
    liveBatteryPct = (uint8_t)(((uint32_t)(liveBatteryMv - 3300) * 100) / (4180 - 3300));
  }
}

// Send 4-byte command packet to all connected BLE Pois
void sendBleCommand(uint8_t cmdCode, uint8_t val) {
  uint8_t pkt[4] = { START_BYTE, cmdCode, val, END_BYTE };
  int sentCount = 0;

  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].rxChar != nullptr) {
      try {
        poiSlots[i].rxChar->writeValue(pkt, 4, false);
        sentCount++;
      } catch(...) {
        Serial.printf("[BLE] Write failed to Poi %d\n", i + 1);
      }
    }
  }
  lastUserActivityTime = millis();
}

// Web App BLE Server Callbacks
class BridgeServerCallbacks : public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    isWebClientConnected = true;
    Serial.println("🌐 [BLE] Open POI Studio Connected to Bridge!");
  }
  void onDisconnect(BLEServer* pServer) {
    isWebClientConnected = false;
    Serial.println("🌐 [BLE] Open POI Studio Disconnected from Bridge!");
    BLEDevice::startAdvertising();
  }
};

// Web App BLE Characteristic Callbacks (Receives UI controls from browser & relays to Pois)
class BridgeRxCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    size_t len = rxValue.length();
    if (len == 0) return;
    const uint8_t *data = (const uint8_t *)rxValue.data();

    if (data[0] == START_BYTE && data[len - 1] == END_BYTE && len >= 3) {
      uint8_t cmdCode = data[1];
      uint8_t val = (len >= 4) ? data[2] : 0;

      if (cmdCode == CC_SET_PATTERN_SLOT) {
        currentSlot = val % 10;
        sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      } else if (cmdCode == CC_SET_BANK) {
        currentBank = val % 5;
        sendBleCommand(CC_SET_BANK, currentBank);
      } else if (cmdCode == CC_SET_PALETTE_FX) {
        currentPalette = val % 33;
        sendBleCommand(CC_SET_PALETTE_FX, currentPalette);
      } else if (cmdCode == CC_SET_MOTION_FX) {
        currentMotion = val % 17;
        sendBleCommand(CC_SET_MOTION_FX, currentMotion);
      } else if (cmdCode == CC_SET_BRIGHTNESS) {
        sendBleCommand(CC_SET_BRIGHTNESS, val);
      }
    }
  }
};

// Power Down to Deep Sleep (Ultra-Low Power OFF: ~0.005 mA)
void enterDeepSleepPowerOff() {
  Serial.println("🛌 [POWER] Entering Deep Sleep (Power OFF)...");

  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].client != nullptr) {
      poiSlots[i].client->disconnect();
    }
  }

  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);

  esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_BUTTON_IN), ESP_GPIO_WAKEUP_GPIO_LOW);

  delay(100);
  esp_deep_sleep_start();
}

// Connect safely in the main Arduino loop task
bool executeConnect(BLEAdvertisedDevice* advDevice) {
  if (advDevice == nullptr) return false;

  int targetSlot = -1;
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (!poiSlots[i].connected) {
      targetSlot = i;
      break;
    }
  }
  if (targetSlot == -1) return false;

  Serial.printf("⚡ [BLE] Connecting to Poi %d: %s (%s)...\n", 
                targetSlot + 1, advDevice->getName().c_str(), advDevice->getAddress().toString().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(advDevice)) {
    Serial.println("❌ [BLE] Failed to connect to device.");
    delete pClient;
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    Serial.println("❌ [BLE] Nordic UART service not found on device.");
    pClient->disconnect();
    delete pClient;
    return false;
  }

  BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteChar == nullptr) {
    Serial.println("❌ [BLE] RX characteristic not found on device.");
    pClient->disconnect();
    delete pClient;
    return false;
  }

  poiSlots[targetSlot].client = pClient;
  poiSlots[targetSlot].rxChar = pRemoteChar;
  poiSlots[targetSlot].connected = true;
  poiSlots[targetSlot].address = advDevice->getAddress().toString();
  poiSlots[targetSlot].name = advDevice->getName();

  Serial.printf("🎉 [BLE] Poi %d LOCKED & READY: %s\n", targetSlot + 1, poiSlots[targetSlot].name.c_str());
  
  // Re-enable advertising to phone after central connection completes
  BLEDevice::startAdvertising();
  return true;
}

// Advertised Device Scanner Callback
class AdvertisedScannerCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = String(advertisedDevice.getName().c_str());
    
    if (devName.indexOf("Bridge") >= 0 || devName.indexOf("Master") >= 0) return;

    bool isMatch = false;
    if (devName.indexOf("OpenPixelPoi") >= 0 || devName.indexOf("Pixel Poi") >= 0 || devName.indexOf("Poi") >= 0) {
      isMatch = true;
    }
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {
      isMatch = true;
    }

    if (isMatch) {
      std::string addr = advertisedDevice.getAddress().toString();
      
      for (int i = 0; i < MAX_BLE_POIS; i++) {
        if (poiSlots[i].connected && poiSlots[i].address == addr) {
          return;
        }
      }

      if (!doConnectPending) {
        if (pendingDevice != nullptr) delete pendingDevice;
        pendingDevice = new BLEAdvertisedDevice(advertisedDevice);
        doConnectPending = true;
        pBLEScan->stop();
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
    Serial.println("  -> Nunchuk Handshake OK: Modern Unencrypted");
    return true;
  }

  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0x40);
  Wire.write(0x00);
  uint8_t err3 = Wire.endTransmission();
  delay(5);

  if (err3 == 0) {
    isNunchukEncrypted = true;
    Serial.println("  -> Nunchuk Handshake OK: Legacy Encrypted");
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

  if (!isNunchukI2cOk && (now - lastI2cRetry > 2000)) {
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

  if (pBridgeTxChar != nullptr && (now - lastTelemetryNotify > 60)) {
    lastTelemetryNotify = now;
    uint8_t telPkt[14];
    telPkt[0] = START_BYTE;
    telPkt[1] = TELEMETRY_BYTE;
    telPkt[2] = joyX;
    telPkt[3] = joyY;
    telPkt[4] = btnZ ? 1 : 0;
    telPkt[5] = btnC ? 1 : 0;
    telPkt[6] = (uint8_t)(accelX >> 2);
    telPkt[7] = (uint8_t)(accelY >> 2);
    telPkt[8] = isNunchukI2cOk ? 1 : 0;
    telPkt[9] = liveBatteryPct;
    telPkt[10] = (uint8_t)(liveBatteryMv >> 8);
    telPkt[11] = (uint8_t)(liveBatteryMv & 0xFF);
    telPkt[12] = isUsbCharging ? 1 : 0;
    telPkt[13] = END_BYTE;
    pBridgeTxChar->setValue(telPkt, 14);
    pBridgeTxChar->notify();
  }

  if (abs((int)joyX - 128) > 30 || abs((int)joyY - 128) > 30 || btnC || btnZ) {
    lastUserActivityTime = now;
  }

  // 1. Joystick X: Slot change (Left / Right flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyX < 50) {
      currentSlot = (currentSlot > 0) ? (currentSlot - 1) : 9;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      lastJoyFlickTime = now;
    } else if (joyX > 200) {
      currentSlot = (currentSlot + 1) % 10;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      lastJoyFlickTime = now;
    }
  }

  // 2. Joystick Y: Bank change (Up / Down flick)
  if (now - lastJoyFlickTime > 250) {
    if (joyY > 200) {
      currentBank = (currentBank + 1) % 5;
      sendBleCommand(CC_SET_BANK, currentBank);
      lastJoyFlickTime = now;
    } else if (joyY < 50) {
      currentBank = (currentBank > 0) ? (currentBank - 1) : 4;
      sendBleCommand(CC_SET_BANK, currentBank);
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
    }
  } else if (btnC && (now - btnCHoldStart > 1200) && btnCHoldStart != 0) {
    currentPalette = 0;
    sendBleCommand(CC_SET_PALETTE_FX, 0);
    btnCHoldStart = 0;
  }

  // 4. Z Trigger: Instant Strobe Drop on hold
  if (btnZ != isStrobeActive) {
    isStrobeActive = btnZ;
    sendBleCommand(CC_SET_MOTION_FX, isStrobeActive ? 15 : 0);
  }

  lastJoyX = joyX;
  lastJoyY = joyY;
  lastBtnC = btnC;
  lastBtnZ = btnZ;
}

// Power Button (D0/D1) Handler
void checkPowerButton() {
  unsigned long now = millis();
  bool isPressed = (digitalRead(PIN_BUTTON_IN) == LOW);

  if (isPressed && !pwrBtnPressed) {
    pwrBtnPressed = true;
    pwrBtnPressStart = now;
  } else if (isPressed && pwrBtnPressed) {
    if (now - pwrBtnPressStart > 1500) {
      enterDeepSleepPowerOff();
    }
  } else if (!isPressed && pwrBtnPressed) {
    pwrBtnPressed = false;
    lastUserActivityTime = now;
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("=================================================");
  Serial.println("Open Pixel Poi - BLE Master Bridge v2.12");
  Serial.println("=================================================");

  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  pinMode(PIN_BUTTON_IN, INPUT_PULLUP);

  lastUserActivityTime = millis();

  autoScanNunchuk();

  // Setup BLE Device with 100% standard name
  BLEDevice::init("OpenPixelBridge");

  // Create Full Nordic UART Server for Web Studio App Connection
  BLEServer *pServer = BLEDevice::createServer();
  pServer->setCallbacks(new BridgeServerCallbacks());

  BLEService *pService = pServer->createService(serviceUUID);

  // RX Characteristic (Web App -> Bridge)
  pBridgeRxChar = pService->createCharacteristic(
    charUUID,
    BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
  );
  pBridgeRxChar->setCallbacks(new BridgeRxCallbacks());

  // TX Characteristic (Bridge -> Web App Telemetry)
  pBridgeTxChar = pService->createCharacteristic(
    txCharUUID,
    BLECharacteristic::PROPERTY_NOTIFY
  );
  pBridgeTxChar->addDescriptor(new BLE2902());

  pService->start();

  // Clean, high-compatibility Advertising payload
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(serviceUUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  // Setup Central Scanner for discovering OpenPixelPoi
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedScannerCallback());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("🎮 BLE Advertising active as 'OpenPixelBridge'! Ready to pair in Chrome.");
}

void loop() {
  unsigned long now = millis();

  updateBatteryTelemetry();
  checkPowerButton();

  if (now - lastUserActivityTime > (10UL * 60UL * 1000UL)) {
    Serial.println("💤 [AUTO-SLEEP] Inactive for 10 minutes. Powering OFF...");
    enterDeepSleepPowerOff();
  }

  if (doConnectPending && pendingDevice != nullptr) {
    executeConnect(pendingDevice);
    delete pendingDevice;
    pendingDevice = nullptr;
    doConnectPending = false;
  }

  int activeCount = 0;
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected) {
      if (poiSlots[i].client == nullptr || !poiSlots[i].client->isConnected()) {
        Serial.printf("[BLE] Poi %d disconnected.\n", i + 1);
        poiSlots[i].connected = false;
        poiSlots[i].rxChar = nullptr;
        if (poiSlots[i].client != nullptr) {
          delete poiSlots[i].client;
          poiSlots[i].client = nullptr;
        }
      } else {
        activeCount++;
      }
    }
  }

  // Non-blocking scan without interrupting advertising
  if (activeCount < MAX_BLE_POIS && !doConnectPending && !isWebClientConnected && (now - lastScanStartTime > 4000)) {
    lastScanStartTime = now;
    pBLEScan->start(1, false);
    BLEDevice::startAdvertising(); // Immediately resume advertising
  }

  readNunchukAndProcess();
  delay(15);
}
