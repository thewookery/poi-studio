/**
 * ============================================================================
 * OPEN PIXEL POI - PURE BLE MASTER NUNCHUK CONTROLLER (v3.1 Funky FX & Gyro)
 * ============================================================================
 * Hardware Controls:
 * - 🕹️ Joystick Left/Right: Switch Pattern Slot (1-10)
 * - 🕹️ Joystick Up/Down: Switch Hardware Bank (1-5)
 * - 🌟 C Button (Tap): Cycle 32 Pro Color Palettes (Rainbow, Sunset, Cyber, etc.)
 * - 🌟 C Button (Hold 1.2s): Reset Palette to Original RGB
 * - ⚡ Z Trigger (Hold): Instant HYPER STROBE BLINDER DROP (Mode 15)
 * - ⚡ Z Trigger (Quick Tap): Step through 16 CRAZY MOTION FLOWS (Warp, Glitch, Laser, Matrix)
 * - 🌀 Gyro Tilt Forward: Speed Boost Hyper-Warp
 * - 🌀 Gyro Tilt Backward: Slow-Mo Motion Flow
 * - 💥 Gyro Whip/Shake: Instant Glitch Shockwave Burst!
 * - 🔘 D0/D1 Button: Tap = Wakeup (3 blinks) | Hold 1.2s = Deep Sleep OFF (2 blinks)
 */

#include <Arduino.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertisedDevice.h>
#include <BLEClient.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

#define PIN_BUTTON_GND 2   // D0 / GPIO 2 - Software Ground
#define PIN_BUTTON_IN  3   // D1 / GPIO 3 - Button Input (Wakeup on LOW)
#define PIN_BOARD_LED  10  // Onboard User LED on Xiao ESP32-C3
#define NUNCHUK_ADDR   0x52

static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

#define START_BYTE 0xD0
#define END_BYTE   0xD1

// Comm Codes
#define CC_SET_BRIGHTNESS   0x02
#define CC_SET_PATTERN_SLOT 0x05
#define CC_SET_BANK         0x07
#define CC_SET_SPEED_OPTION 0x0C
#define CC_SET_PALETTE_FX   0x15
#define CC_SET_MOTION_FX    0x18

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
uint8_t currentSpeedOpt = 2; // default normal speed (Level 3)
bool isStrobeActive = false;

// Nunchuk Live State
uint8_t liveJoyX = 128;
uint8_t liveJoyY = 128;
bool liveBtnC = false;
bool liveBtnZ = false;
bool lastBtnC = false;
bool lastBtnZ = false;
int16_t liveAccelX = 512;
int16_t liveAccelY = 512;
int16_t liveAccelZ = 512;

unsigned long btnCHoldStart = 0;
unsigned long btnZPressStart = 0;
unsigned long lastJoyFlickTime = 0;
unsigned long lastTiltCheckTime = 0;
unsigned long lastShakeTime = 0;
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

// Visual LED Animations
void flashWakeupLed() {
  pinMode(PIN_BOARD_LED, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BOARD_LED, LOW);
    delay(75);
    digitalWrite(PIN_BOARD_LED, HIGH);
    delay(75);
  }
}

void flashPowerOffLed() {
  pinMode(PIN_BOARD_LED, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BOARD_LED, LOW);
    delay(280);
    digitalWrite(PIN_BOARD_LED, HIGH);
    delay(150);
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
  Serial.printf("[BLE] Sent 0x%02X -> Val: %d to %d Poi(s)\n", cmdCode, val, sentCount);
  lastUserActivityTime = millis();
}

// Enter Ultra-Low Power Deep Sleep (Power OFF: ~0.005 mA)
void enterDeepSleepPowerOff() {
  Serial.println("🛌 [POWER] Powering OFF (Entering Deep Sleep)...");
  flashPowerOffLed();

  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].client != nullptr) {
      poiSlots[i].client->disconnect();
    }
  }

  gpio_hold_dis((gpio_num_t)PIN_BUTTON_GND);
  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  gpio_hold_en((gpio_num_t)PIN_BUTTON_GND);
  gpio_deep_sleep_hold_en();

  esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_BUTTON_IN), ESP_GPIO_WAKEUP_GPIO_LOW);

  delay(50);
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
  
  digitalWrite(PIN_BOARD_LED, LOW);
  delay(100);
  digitalWrite(PIN_BOARD_LED, HIGH);
  delay(100);
  digitalWrite(PIN_BOARD_LED, LOW);
  delay(100);
  digitalWrite(PIN_BOARD_LED, HIGH);

  return true;
}

// Advertised Device Scanner Callback
class AdvertisedScannerCallback: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    String devName = String(advertisedDevice.getName().c_str());
    
    bool isMatch = false;
    if (devName.indexOf("OpenPixelPoi") >= 0 || devName.indexOf("Pixel Poi") >= 0 || devName.indexOf("DotStar") >= 0 || devName.indexOf("Poi") >= 0) {
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

  if (abs((int)joyX - 128) > 30 || abs((int)joyY - 128) > 30 || btnC || btnZ || abs(accelX - 512) > 150) {
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

  // 3. C Button: Step Palette or Reset
  if (btnC && !lastBtnC) {
    btnCHoldStart = now;
  } else if (!btnC && lastBtnC) {
    unsigned long pressDur = now - btnCHoldStart;
    if (pressDur < 800) {
      currentPalette = (currentPalette + 1) % 33;
      sendBleCommand(CC_SET_PALETTE_FX, currentPalette);
      Serial.printf("🎨 [NUNCHUK] Switched to Palette %d\n", currentPalette);
    }
  } else if (btnC && (now - btnCHoldStart > 1000) && btnCHoldStart != 0) {
    currentPalette = 0;
    sendBleCommand(CC_SET_PALETTE_FX, 0);
    Serial.println("🎨 [NUNCHUK] Reset Palette to Original RGB!");
    btnCHoldStart = 0;
  }

  // 4. Z Trigger: Dual Action (Hold = Strobe Blinder Drop | Tap = Cycle 16 Crazy Motion Flows!)
  if (btnZ && !lastBtnZ) {
    btnZPressStart = now;
    isStrobeActive = false;
  } else if (btnZ && (now - btnZPressStart > 300) && !isStrobeActive) {
    // Squeeze & Hold > 300ms -> Instant Hyper Strobe Blinder Drop!
    isStrobeActive = true;
    sendBleCommand(CC_SET_MOTION_FX, 15); // Mode 15: Hyper Strobe Blinder
    Serial.println("⚡ [STROBE DROP] Hyper Strobe Blinder ACTIVE!");
  } else if (!btnZ && lastBtnZ) {
    if (isStrobeActive) {
      // Released Strobe Hold -> Snap back to current motion flow (or Solid 0)
      isStrobeActive = false;
      sendBleCommand(CC_SET_MOTION_FX, currentMotion);
      Serial.println("⚡ [STROBE DROP] Strobe Released -> Restoring pattern.");
    } else {
      // Quick Tap < 300ms -> Step through 16 Crazy Motion Flows!
      currentMotion = (currentMotion + 1) % 17;
      sendBleCommand(CC_SET_MOTION_FX, currentMotion);
      Serial.printf("🌀 [MOTION FLOW] Switched to Motion FX Flow Mode %d\n", currentMotion);
    }
  }

  // 5. Gyro Tilt Speed & Shake Burst Modulation (Every 120ms)
  if (now - lastTiltCheckTime > 120) {
    lastTiltCheckTime = now;

    // A. Shake / Whip Detection (Sudden acceleration jerk)
    int16_t jerkX = abs(accelX - 512);
    int16_t jerkY = abs(accelY - 512);
    int16_t jerkZ = abs(accelZ - 512);
    if ((jerkX > 280 || jerkY > 280 || jerkZ > 320) && (now - lastShakeTime > 1500)) {
      lastShakeTime = now;
      // Fire an instant Glitch Shockwave Drop (Mode 8) for 700ms!
      sendBleCommand(CC_SET_MOTION_FX, 8); // Warp Shockwave
      Serial.println("💥 [GYRO SHAKE] Wild Shockwave Drop Triggered!");
    } else if (now - lastShakeTime > 750 && now - lastShakeTime < 950) {
      sendBleCommand(CC_SET_MOTION_FX, currentMotion); // Restore motion mode
    }

    // B. Pitch Tilt Speed Modulation (Tilt forward = Hyper Speed | Tilt back = Slow-Mo)
    uint8_t targetSpeed = 2; // Level 3 (Normal default)
    if (accelY > 640) {
      targetSpeed = 5; // Level 6 (Hyper Warp Speed Boost!)
    } else if (accelY > 580) {
      targetSpeed = 4; // Level 5 (Fast)
    } else if (accelY < 380) {
      targetSpeed = 0; // Level 1 (Smooth Slow-Mo Flow)
    } else if (accelY < 440) {
      targetSpeed = 1; // Level 2 (Relaxed Flow)
    }

    if (targetSpeed != currentSpeedOpt) {
      currentSpeedOpt = targetSpeed;
      sendBleCommand(CC_SET_SPEED_OPTION, currentSpeedOpt);
      Serial.printf("🚀 [GYRO SPEED] Dynamic Tilt Speed: Level %d\n", currentSpeedOpt + 1);
    }
  }

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
    if (now - pwrBtnPressStart > 1200) {
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
  Serial.println("Open Pixel Poi - BLE Master Nunchuk v3.1 (Funky FX)");
  Serial.println("=================================================");

  gpio_hold_dis((gpio_num_t)PIN_BUTTON_GND);

  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  pinMode(PIN_BUTTON_IN, INPUT_PULLUP);

  flashWakeupLed();

  lastUserActivityTime = millis();

  autoScanNunchuk();

  BLEDevice::init("OpenPixelMasterCentral");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedScannerCallback());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  Serial.println("🎮 Auto-Scanning for OpenPixelPoi over Bluetooth...");
}

void loop() {
  unsigned long now = millis();

  // 1. Check D0 / D1 Power Button
  checkPowerButton();

  // 2. Auto-Sleep after 10 minutes of inactivity
  if (now - lastUserActivityTime > (10UL * 60UL * 1000UL)) {
    Serial.println("💤 [AUTO-SLEEP] Inactive for 10 minutes. Powering OFF...");
    enterDeepSleepPowerOff();
  }

  // 3. Connect to discovered Pois
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

  if (activeCount < MAX_BLE_POIS && !doConnectPending && (now - lastScanStartTime > 3000)) {
    lastScanStartTime = now;
    pBLEScan->start(2, false);
  }

  // 5. Process Wii Nunchuk
  readNunchukAndProcess();
  delay(15);
}
