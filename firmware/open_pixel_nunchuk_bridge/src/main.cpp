/**
 * ============================================================================
 * OPEN PIXEL POI - PURE BLE MASTER NUNCHUK BRIDGE (v2.9)
 * ============================================================================
 * Hardware Layout:
 * - Battery: 18350 3.7V LiPo on BAT+ / BAT- pads (Back of board)
 * - Power Button: Connected across D0 (Pin 1 / GPIO 2) and D1 (Pin 2 / GPIO 3)
 *   - Short Press: Wakes up / changes mode
 *   - Long Press (1.5s): Enters Ultra-Low Power Deep Sleep (Power OFF)
 *   - Auto-Sleep: 10 minutes of inactivity
 * - Nunchuk I2C: 3V3, GND, D2 (SDA / GPIO 4), D3 (SCL / GPIO 5)
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include "esp_sleep.h"

#define PIN_BUTTON_GND D0  // GPIO 2 - Software Ground
#define PIN_BUTTON_IN  D1  // GPIO 3 - Button Input (Pulled UP)
#define NUNCHUK_ADDR   0x52

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

unsigned long btnCHoldStart = 0;
unsigned long lastJoyFlickTime = 0;
unsigned long lastI2cRetry = 0;
unsigned long lastScanStartTime = 0;
unsigned long lastUserActivityTime = 0;
unsigned long pwrBtnPressStart = 0;
bool pwrBtnPressed = false;

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
  Serial.printf("[BLE CMD] 0x%02X -> Val: %d (Sent to %d Poi)\n", cmdCode, val, sentCount);
  lastUserActivityTime = millis();
}

// Power Down to Deep Sleep (Ultra-Low Power OFF: ~0.005 mA)
void enterDeepSleepPowerOff() {
  Serial.println("🛌 [POWER] Entering Deep Sleep (Power OFF)...");

  // Disconnect BLE cleanly
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].client != nullptr) {
      poiSlots[i].client->disconnect();
    }
  }

  // Ensure D0 is LOW as ground
  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);

  // Configure D1 (GPIO 3) as wakeup source on LOW
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
  return true;
}

// Advertised Device Scanner Callback
class AdvertisedScannerCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = String(advertisedDevice.getName().c_str());
    
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
  bool btnZ = !((raw[5] >> 0) & 0x01);
  bool btnC = !((raw[5] >> 1) & 0x01);

  if (joyX == 0 && joyY == 0 && raw[2] == 0) {
    isNunchukI2cOk = false;
    return;
  }

  // Activity detection for auto-sleep
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
      // Held for 1.5s -> Power OFF!
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
  Serial.println("Open Pixel Poi - BLE Master Nunchuk (D0/D1 Power)");
  Serial.println("=================================================");

  // Setup D0 as Software Ground and D1 as Input Pullup
  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  pinMode(PIN_BUTTON_IN, INPUT_PULLUP);

  lastUserActivityTime = millis();

  autoScanNunchuk();

  BLEDevice::init("OpenPixelNunchukMaster");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedScannerCallback());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("🎮 Auto-Scanning for OpenPixelPoi over Bluetooth...");
}

void loop() {
  unsigned long now = millis();

  // 1. Check Power Button (D0 / D1)
  checkPowerButton();

  // 2. Auto-Sleep after 10 minutes of inactivity
  if (now - lastUserActivityTime > (10UL * 60UL * 1000UL)) {
    Serial.println("💤 [AUTO-SLEEP] Inactive for 10 minutes. Powering OFF...");
    enterDeepSleepPowerOff();
  }

  // 3. Process pending BLE connection safely in loop task
  if (doConnectPending && pendingDevice != nullptr) {
    executeConnect(pendingDevice);
    delete pendingDevice;
    pendingDevice = nullptr;
    doConnectPending = false;
  }

  // 4. Health check connections & trigger background scan if slot is available
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

  if (activeCount < MAX_BLE_POIS && !doConnectPending && (now - lastScanStartTime > 3500)) {
    lastScanStartTime = now;
    pBLEScan->start(2, false);
  }

  // 5. Process Wii Nunchuk inputs
  readNunchukAndProcess();
  delay(15);
}
