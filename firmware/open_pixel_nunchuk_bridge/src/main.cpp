/**
 * ============================================================================
 * OPEN PIXEL POI - MASTER NUNCHUK BLE BRIDGE (Build v143)
 * ============================================================================
 * Z-Hold Step 2 Motion Flow Remix & Barrel Roll Tilt Speed Throttle
 * 
 * 🎮 CONTROLS:
 * - 📱 Auto-Pairs to OpenPixelPoi over Bluetooth in 1 second
 * 
 * 🕹️ NORMAL MODE (When Z is NOT held):
 * - 🕹️ Joystick Left / Right: Previous / Next Pattern Slot (1-10)
 * - 🕹️ Joystick Up / Down: Previous / Next Hardware Bank (1-5)
 * - 🌟 C-Button (Tap): Step through Step 1 Color Palettes (1-32)
 * - 🌟 C-Button (Hold 1.0s): Reset Palette to Original True RGB
 * 
 * ⚡ Z-HOLD STEP 2 REMIX LAYER (Hold Z):
 * - 🕹️ Hold Z + Joystick Left / Right: Flick through all 16 Step 2 Motion Flows!
 *     (1=Flow Up, 2=Flow Down, 3=Matrix Rain, 4=Tidal Wave, 5=Plasma, 6=Stardust,
 *      7=Chroma Pulse, 8=POV Spin, 9=Spiral Helix, 10=Strobe, 11=Warp Pulse,
 *      12=Glitch Spark, 13=Aurora Wave, 14=Lava Flare, 15=Disco Strobe, 16=Black Hole)
 * - 💥 Hold Z + Wrist Flick (Sharp Snap): Instant next random Motion Flow FX drop!
 * - 🌀 Hold Z + Barrel Roll (Tilt Left / Right): Dynamically throttles flow speed (3 to 10)!
 * - 🔄 Release Z: Instantly & fluidly restores steady stock pattern playback in 0ms!
 * 
 * 🔘 D0/D1 Power Button: Tap = Wakeup (3 blinks) | Hold 1.2s = Deep Sleep OFF (2 blinks)
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

#define START_BYTE 0xD0
#define END_BYTE   0xD1

// Safe CommCodes
#define CC_SET_BRIGHTNESS        0x02  // 2
#define CC_SET_SPEED             0x03  // 3
#define CC_SET_PATTERN_SLOT      0x05  // 5
#define CC_SET_BANK              0x07  // 7
#define CC_SET_BRIGHTNESS_OPTION 0x10  // 16 (0 to 5)
#define CC_SET_SPEED_OPTION      0x12  // 18 (0 to 5)
#define CC_SET_PALETTE_FX        0x15  // 21 (0 to 32)
#define CC_SET_PALETTE_SPEED     0x17  // 23 (1 to 10)
#define CC_SET_MOTION_FX         0x18  // 24 (0 to 17)

// Nordic UART Service (NUS)
static BLEUUID serviceUUID("6e400001-b5a3-f393-e0a9-e50e24dcca9e");
static BLEUUID charUUID("6e400002-b5a3-f393-e0a9-e50e24dcca9e");

struct PoiSlot {
  BLEClient* client = nullptr;
  BLERemoteCharacteristic* rxChar = nullptr;
  bool connected = false;
  std::string address = "";
  std::string name = "";
};

#define MAX_BLE_POIS 4
PoiSlot poiSlots[MAX_BLE_POIS];

unsigned long scanWindowEndTime = 0;

void startDiscoveryWindow(unsigned long durationMs = 25000) {
  scanWindowEndTime = millis() + durationMs;
  Serial.printf("📡 [BLE] Discovery Window OPEN for %lu ms (Scanning active)\n", durationMs);
}

BLEScan* pBLEScan = nullptr;
bool doConnectPending = false;
BLEAdvertisedDevice* pendingDevice = nullptr;
unsigned long lastScanStartTime = 0;

struct I2CPair {
  uint8_t sda;
  uint8_t scl;
  const char* name;
};

const I2CPair I2C_PAIRS[] = {
  { 5, 4, "D3 (SDA) / D2 (SCL)" },
  { 4, 5, "D2 (SDA) / D3 (SCL)" },
  { 6, 7, "D4 (SDA) / D5 (SCL)" },
  { 7, 6, "D5 (SDA) / D4 (SCL)" }
};
const int NUM_I2C_PAIRS = sizeof(I2C_PAIRS) / sizeof(I2C_PAIRS[0]);

int activeI2cIndex = 0;
bool isNunchukEncrypted = false;
bool isNunchukI2cOk = false;

// State Variables
uint8_t currentSlot = 0;
uint8_t currentBank = 0;
uint8_t currentPalette = 0;
// Brightness Gesture State (Hold Z + C simultaneously)
bool isBrightnessComboActive = false;
uint8_t currentBrightnessOption = 4; // Default ~50%
unsigned long lastBrightnessStepTime = 0;

uint8_t activeZMotion = 1; // Default Step 2 Motion: Flow UP

// Pre-Z Snapshot for 100% Perfect State Restoration
uint8_t preZPalette = 0;
uint8_t preZMotion = 0;

// Gesture & Filtering State
bool isZHeld = false;
uint8_t activeZSpeed = 255;
unsigned long lastTiltSampleTime = 0;
unsigned long lastFlickTime = 0;

// Low-Pass Filtered Accelerometer for Silky Smooth Barrel Roll
float smoothedAccelX = 512.0f;
int16_t prevAccelX = 512;
int16_t prevAccelY = 512;
int16_t prevAccelZ = 700;

// Nunchuk Live State
uint8_t lastJoyX = 128;
uint8_t lastJoyY = 128;
bool lastBtnC = false;
bool lastBtnZ = false;
unsigned long btnCHoldStart = 0;
unsigned long lastJoyFlickTime = 0;
unsigned long lastI2cRetry = 0;
unsigned long lastUserActivityTime = 0;
unsigned long pwrBtnPressStart = 0;
bool pwrBtnPressed = false;

void flashWakeupLed() {
  pinMode(PIN_BOARD_LED, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(PIN_BOARD_LED, LOW);
    delay(75);
    digitalWrite(PIN_BOARD_LED, HIGH);
    delay(75);
  }
}

void sendBleCommand(uint8_t cmdCode, uint8_t val);

void flashPowerOffLed() {
  pinMode(PIN_BOARD_LED, OUTPUT);
  for (int i = 0; i < 2; i++) {
    digitalWrite(PIN_BOARD_LED, LOW);
    delay(280);
    digitalWrite(PIN_BOARD_LED, HIGH);
    delay(150);
  }
}

void enterDeepSleepPowerOff() {
  Serial.println("🛌 [POWER] Gracefully disconnecting BLE and Powering OFF...");

  // 1. Send clean baseline reset command to Poi props before shutting down
  sendBleCommand(CC_SET_MOTION_FX, 0);
  sendBleCommand(CC_SET_PALETTE_FX, 0);
  sendBleCommand(CC_SET_PALETTE_SPEED, 5);
  delay(120);

  // 2. Gracefully disconnect all BLE clients so the Poi never gets stuck
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].client != nullptr) {
      try {
        poiSlots[i].client->disconnect();
      } catch (...) {}
      poiSlots[i].connected = false;
      poiSlots[i].rxChar = nullptr;
    }
  }
  delay(180);

  flashPowerOffLed();

  gpio_hold_dis((gpio_num_t)PIN_BUTTON_GND);
  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  gpio_hold_en((gpio_num_t)PIN_BUTTON_GND);
  gpio_deep_sleep_hold_en();

  esp_deep_sleep_enable_gpio_wakeup((1ULL << PIN_BUTTON_IN), ESP_GPIO_WAKEUP_GPIO_LOW);

  delay(50);
  esp_deep_sleep_start();
}

// Send standard 4-byte command to connected Poi props
void sendBleCommand(uint8_t cmdCode, uint8_t val) {
  uint8_t pkt[4] = { START_BYTE, cmdCode, val, END_BYTE };
  lastUserActivityTime = millis();

  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (poiSlots[i].connected && poiSlots[i].rxChar != nullptr) {
      try {
        poiSlots[i].rxChar->writeValue(pkt, 4, false);
      } catch (...) {
        Serial.printf("❌ [BLE] Send failed to Poi %d\n", i + 1);
      }
    }
  }
}

// Connect to discovered Poi
bool executeConnect(BLEAdvertisedDevice* advDevice) {
  int targetSlot = -1;
  for (int i = 0; i < MAX_BLE_POIS; i++) {
    if (!poiSlots[i].connected) {
      targetSlot = i;
      break;
    }
  }
  if (targetSlot == -1) return false;

  Serial.printf("🔗 [BLE] Auto-Connecting to %s...\n", advDevice->getName().c_str());

  BLEClient* pClient = BLEDevice::createClient();
  if (!pClient->connect(advDevice)) {
    delete pClient;
    return false;
  }

  BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
  if (pRemoteService == nullptr) {
    pClient->disconnect();
    delete pClient;
    return false;
  }

  BLERemoteCharacteristic* pRemoteChar = pRemoteService->getCharacteristic(charUUID);
  if (pRemoteChar == nullptr) {
    pClient->disconnect();
    delete pClient;
    return false;
  }

  poiSlots[targetSlot].client = pClient;
  poiSlots[targetSlot].rxChar = pRemoteChar;
  poiSlots[targetSlot].connected = true;
  poiSlots[targetSlot].address = advDevice->getAddress().toString();
  poiSlots[targetSlot].name = advDevice->getName();

  Serial.printf("🎉 [BLE] Poi %d LOCKED & READY: %s!\n", targetSlot + 1, poiSlots[targetSlot].name.c_str());
  return true;
}

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
    return true;
  }

  Wire.beginTransmission(NUNCHUK_ADDR);
  Wire.write(0x40);
  Wire.write(0x00);
  uint8_t err3 = Wire.endTransmission();
  delay(5);

  if (err3 == 0) {
    isNunchukEncrypted = true;
    return true;
  }

  return false;
}

void autoScanNunchuk() {
  for (int i = 0; i < NUM_I2C_PAIRS; i++) {
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

  if (abs((int)joyX - 128) > 30 || abs((int)joyY - 128) > 30 || btnC || btnZ) {
    lastUserActivityTime = now;
  }

  // Low-Pass Filter on AccelX for Smooth Throttle
  smoothedAccelX = (smoothedAccelX * 0.70f) + ((float)accelX * 0.30f);

  // =========================================================================
  // 💡 0. BRIGHTNESS COMBO (HOLD Z + C SIMULTANEOUSLY)
  // =========================================================================
  bool isZandCHeld = (btnZ && btnC);

  if (isZandCHeld) {
    isBrightnessComboActive = true;

    // A. Joystick Up / Down for step adjustment
    if (now - lastBrightnessStepTime > 250) {
      if (joyY > 200) {
        if (currentBrightnessOption < 5) {
          currentBrightnessOption++;
          sendBleCommand(CC_SET_BRIGHTNESS_OPTION, currentBrightnessOption);
          Serial.printf("💡 [BRIGHTNESS (Z+C)] Level: %d/5\n", currentBrightnessOption);
        }
        lastBrightnessStepTime = now;
      } else if (joyY < 55) {
        if (currentBrightnessOption > 0) {
          currentBrightnessOption--;
          sendBleCommand(CC_SET_BRIGHTNESS_OPTION, currentBrightnessOption);
          Serial.printf("💡 [BRIGHTNESS (Z+C)] Level: %d/5\n", currentBrightnessOption);
        }
        lastBrightnessStepTime = now;
      }
    }

    // B. Pitch Tilt (accelY) for intuitive gesture adjustment
    if (now - lastTiltSampleTime > 120) {
      lastTiltSampleTime = now;
      int tiltY = accelY - 512;
      if (abs(tiltY) > 80 && (now - lastBrightnessStepTime > 320)) {
        if (tiltY > 80 && currentBrightnessOption < 5) {
          currentBrightnessOption++;
          sendBleCommand(CC_SET_BRIGHTNESS_OPTION, currentBrightnessOption);
          Serial.printf("💡 [TILT BRIGHTNESS] Level: %d/5\n", currentBrightnessOption);
          lastBrightnessStepTime = now;
        } else if (tiltY < -80 && currentBrightnessOption > 0) {
          currentBrightnessOption--;
          sendBleCommand(CC_SET_BRIGHTNESS_OPTION, currentBrightnessOption);
          Serial.printf("💡 [TILT BRIGHTNESS] Level: %d/5\n", currentBrightnessOption);
          lastBrightnessStepTime = now;
        }
      }
    }

    lastBtnZ = btnZ;
    lastBtnC = btnC;
    prevAccelX = accelX;
    prevAccelY = accelY;
    prevAccelZ = accelZ;
    return;
  }

  // Clear combo latch on release
  if (!btnZ && !btnC && isBrightnessComboActive) {
    isBrightnessComboActive = false;
  }

  // =========================================================================
  // ⚡ 1. Z-TRIGGER GESTURE ENGINE (Hold Z + Step 2 Remix / Tilt Speed Throttle)
  // =========================================================================
  if (btnZ && !lastBtnZ) {
    // Save snapshot of current settings before Z engagement
    preZPalette = currentPalette;
    preZMotion = 0;

    isZHeld = true;
    activeZSpeed = 255;
    smoothedAccelX = (float)accelX;

    // Activate the current Step 2 Motion Flow
    if (activeZMotion == 0) activeZMotion = 1;
    sendBleCommand(CC_SET_MOTION_FX, activeZMotion);
    sendBleCommand(CC_SET_PALETTE_SPEED, 6);
    Serial.printf("⚡ [Z-ENGAGE] Step 2 Motion %d Active!\n", activeZMotion);
  } else if (btnZ) {

    // 💥 1A. WRIST FLICK (Sharp Snap) -> Instant Next Step 2 Motion Flow Drop!
    int16_t deltaX = abs(accelX - prevAccelX);
    int16_t deltaY = abs(accelY - prevAccelY);
    int16_t deltaZ = abs(accelZ - prevAccelZ);
    int16_t totalJerk = deltaX + deltaY + deltaZ;

    if (totalJerk > 200 && (now - lastFlickTime > 400)) {
      lastFlickTime = now;
      activeZMotion = (activeZMotion % 16) + 1; // 1 to 16
      sendBleCommand(CC_SET_MOTION_FX, activeZMotion);
      Serial.printf("💥 [WRIST FLICK] Jumped to Step 2 Motion %d!\n", activeZMotion);
    }

    // 🕹️ 1B. HOLD Z + JOYSTICK LEFT/RIGHT -> FLICK THROUGH STEP 2 MOTION FLOWS (1 to 16)
    if (now - lastJoyFlickTime > 280) {
      if (joyX < 50) {
        // Previous Step 2 Motion Flow (1 to 16)
        activeZMotion = (activeZMotion > 1) ? (activeZMotion - 1) : 16;
        sendBleCommand(CC_SET_MOTION_FX, activeZMotion);
        Serial.printf("◀️ [Z-COMBO] Step 2 Motion: %d\n", activeZMotion);
        lastJoyFlickTime = now;
      } else if (joyX > 200) {
        // Next Step 2 Motion Flow (1 to 16)
        activeZMotion = (activeZMotion % 16) + 1;
        sendBleCommand(CC_SET_MOTION_FX, activeZMotion);
        Serial.printf("▶️ [Z-COMBO] Step 2 Motion: %d\n", activeZMotion);
        lastJoyFlickTime = now;
      }
    }

    // 🌀 1C. HOLD Z + BARREL ROLL TILT -> DYNAMIC FLOW SPEED THROTTLE
    if (now - lastTiltSampleTime > 35) {
      lastTiltSampleTime = now;
      float dev = abs(smoothedAccelX - 512.0f); // Tilt magnitude
      uint8_t targetSpeed = 4;
      if (dev > 30.0f) {
        float mag = (dev - 30.0f) / 220.0f;
        if (mag > 1.0f) mag = 1.0f;
        targetSpeed = 4 + (uint8_t)(mag * 6.0f); // 4 to 10 (Dynamic Throttle)
      } else {
        targetSpeed = 4; // Steady / Chill
      }

      if (targetSpeed != activeZSpeed) {
        activeZSpeed = targetSpeed;
        sendBleCommand(CC_SET_PALETTE_SPEED, targetSpeed);
      }
    }

  } else if (!btnZ && lastBtnZ) {
    // 🔄 RELEASE Z -> PERFECTLY RESTORE BASELINE STEADY PATTERN!
    isZHeld = false;
    sendBleCommand(CC_SET_MOTION_FX, 0);                 // Motion OFF
    sendBleCommand(CC_SET_PALETTE_SPEED, 5);              // Base speed
    sendBleCommand(CC_SET_PALETTE_FX, preZPalette);       // Restore palette
    currentPalette = preZPalette;
    Serial.printf("🔄 [Z-RELEASE] Fluidly restored steady pattern (Pal: %d)\n", preZPalette);
  }

  prevAccelX = accelX;
  prevAccelY = accelY;
  prevAccelZ = accelZ;

  // =========================================================================
  // 🕹️ 2. JOYSTICK PROCESSING (Filtered Slot & Bank Navigation when not holding Z)
  // =========================================================================
  // Ignore noisy I2C disconnect readings (0 or 255)
  bool joyValid = (joyX > 15 && joyX < 240 && joyY > 15 && joyY < 240);

  if (!isZHeld && joyValid && (now - lastJoyFlickTime > 350)) {
    if (joyX < 55) {
      // Previous Pattern Slot (1-10)
      currentSlot = (currentSlot > 0) ? (currentSlot - 1) : 9;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      Serial.printf("◀️ [JOYSTICK] Pattern Slot: %d\n", currentSlot + 1);
      lastJoyFlickTime = now;
    } else if (joyX > 200) {
      // Next Pattern Slot (1-10)
      currentSlot = (currentSlot + 1) % 10;
      sendBleCommand(CC_SET_PATTERN_SLOT, currentSlot);
      Serial.printf("▶️ [JOYSTICK] Pattern Slot: %d\n", currentSlot + 1);
      lastJoyFlickTime = now;
    } else if (joyY > 200) {
      // Next Hardware Bank (1-5)
      currentBank = (currentBank + 1) % 5;
      sendBleCommand(CC_SET_BANK, currentBank);
      Serial.printf("🔼 [JOYSTICK] Bank: %d\n", currentBank + 1);
      lastJoyFlickTime = now;
    } else if (joyY < 55) {
      // Previous Hardware Bank (1-5)
      currentBank = (currentBank > 0) ? (currentBank - 1) : 4;
      sendBleCommand(CC_SET_BANK, currentBank);
      Serial.printf("🔽 [JOYSTICK] Bank: %d\n", currentBank + 1);
      lastJoyFlickTime = now;
    }
  }

  // =========================================================================
  // 🌟 3. C BUTTON (Tap = Step Palette | Hold 1.0s = Reset Original RGB)
  // =========================================================================
  if (!isZHeld) {
    if (btnC && !lastBtnC) {
      btnCHoldStart = now;
    } else if (!btnC && lastBtnC) {
      unsigned long pressDur = now - btnCHoldStart;
      if (pressDur < 800) {
        startDiscoveryWindow(20000); // Re-open discovery if new props turned on
        currentPalette = (currentPalette + 1) % 33;
        sendBleCommand(CC_SET_PALETTE_FX, currentPalette);
        Serial.printf("🎨 [C-BUTTON] Palette %d\n", currentPalette);
      }
    } else if (btnC && (now - btnCHoldStart > 1000) && btnCHoldStart != 0) {
      currentPalette = 0;
      sendBleCommand(CC_SET_PALETTE_FX, 0);
      Serial.println("🎨 [C-BUTTON] Reset Palette to Original RGB!");
      btnCHoldStart = 0;
    }
  }

  lastBtnC = btnC;
  lastBtnZ = btnZ;
}

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
  Serial.println("Open Pixel Poi - Master Nunchuk BLE Bridge (v143)");
  Serial.println("=================================================");

  gpio_hold_dis((gpio_num_t)PIN_BUTTON_GND);

  pinMode(PIN_BUTTON_GND, OUTPUT);
  digitalWrite(PIN_BUTTON_GND, LOW);
  pinMode(PIN_BUTTON_IN, INPUT_PULLUP);

  flashWakeupLed();

  lastUserActivityTime = millis();
  autoScanNunchuk();

  BLEDevice::init("OpenPixelNunchukMaster");
  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new AdvertisedScannerCallback());
  pBLEScan->setInterval(1349);
  pBLEScan->setWindow(449);
  pBLEScan->setActiveScan(true);

  startDiscoveryWindow(25000);
  Serial.println("🎮 Auto-Scanning for OpenPixelPoi over Bluetooth (25s Window)...");
}

void loop() {
  unsigned long now = millis();
  checkPowerButton();

  // Power-on 1.5s warmup buffer: let BLE radio and power stabilize before sending commands
  if (now < 1500) {
    delay(20);
    return;
  }

  // 1. Process pending BLE connection
  if (doConnectPending && pendingDevice != nullptr) {
    executeConnect(pendingDevice);
    delete pendingDevice;
    pendingDevice = nullptr;
    doConnectPending = false;
  }

  // 2. Health check connections & trigger background scan
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

  // If a prop disconnected, immediately reopen 20s discovery window
  static int prevActiveCount = 0;
  if (activeCount < prevActiveCount) {
    startDiscoveryWindow(20000);
  }
  prevActiveCount = activeCount;

  // Scan strictly within the active discovery window to eliminate hitches
  bool isScanWindowActive = (now < scanWindowEndTime);
  if (activeCount < MAX_BLE_POIS && isScanWindowActive && !doConnectPending && (now - lastScanStartTime > 3500)) {
    lastScanStartTime = now;
    pBLEScan->start(1, false);
  }

  if (now - lastUserActivityTime > (10UL * 60UL * 1000UL)) {
    Serial.println("💤 [AUTO-SLEEP] Inactive for 10 minutes. Powering OFF...");
    enterDeepSleepPowerOff();
  }

  readNunchukAndProcess();
  delay(10);
}
