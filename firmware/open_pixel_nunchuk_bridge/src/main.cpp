#include <Arduino.h>
#include <Wire.h>
#include <WiFi.h>
#include <esp_now.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

#define I2C_SDA_PIN 4
#define I2C_SCL_PIN 5
#define NUNCHUK_ADDR 0x52

#define ESPNOW_PACKET_MAGIC 0xA5

// BLE UUIDs matching Open Pixel Poi GATT
#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

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
bool isStrobeActive = false;

// Nunchuk State
uint8_t lastJoyX = 128;
uint8_t lastJoyY = 128;
bool lastBtnC = false;
bool lastBtnZ = false;
unsigned long btnCHoldStart = 0;
unsigned long lastJoyFlickTime = 0;

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

// BLE Callback: forwards any app commands directly over ESP-NOW
class BridgeBleCallbacks : public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();
    if (rxValue.length() >= 2) {
      uint8_t code = (uint8_t)rxValue[1];
      uint8_t val = (rxValue.length() >= 3) ? (uint8_t)rxValue[2] : 0;

      // Handle common Open POI Studio commands
      if (code == 0x01) { // Set Pattern Slot
        currentSlot = val % 10;
        sendEspNowPacket(CMD_SET_PATTERN, currentSlot);
      } else if (code == 0x02) { // Set Bank
        currentBank = val % 5;
        sendEspNowPacket(CMD_SET_BANK, currentBank);
      } else if (code == 0x07) { // Set Brightness
        sendEspNowPacket(CMD_SET_BRIGHTNESS, val);
      } else if (code == 0x10) { // Set Palette
        currentPalette = val % 33;
        sendEspNowPacket(CMD_SET_PALETTE, currentPalette);
      } else if (code == 0x13) { // Set Motion FX
        currentMotion = val % 17;
        sendEspNowPacket(CMD_SET_MOTION_FX, currentMotion);
      }
    }
  }
};

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

void setupEspNowBridge() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  if (esp_now_init() != ESP_OK) {
    Serial.println("ESP-NOW Bridge Init Failed!");
    return;
  }

  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, broadcastMac, 6);
  peerInfo.channel = 1;
  peerInfo.encrypt = false;
  esp_now_add_peer(&peerInfo);

  Serial.println("ESP-NOW Bridge Broadcast Ready on Channel 1!");
}

void setupBleGateway() {
  BLEDevice::init("OpenPixelBridge-Nunchuk");
  BLEServer *pServer = BLEDevice::createServer();
  BLEService *pService = pServer->createService(SERVICE_UUID);
  BLECharacteristic *pCharacteristic = pService->createCharacteristic(
    CHARACTERISTIC_UUID,
    BLECharacteristic::PROPERTY_READ |
    BLECharacteristic::PROPERTY_WRITE |
    BLECharacteristic::PROPERTY_NOTIFY
  );

  pCharacteristic->setCallbacks(new BridgeBleCallbacks());
  pCharacteristic->addDescriptor(new BLE2902());
  pService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true);
  pAdvertising->setMinPreferred(0x06);
  pAdvertising->setMinPreferred(0x12);
  BLEDevice::startAdvertising();

  Serial.println("Bridge BLE Gateway Active & Advertising!");
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
  Serial.println("Starting Open Pixel Master Nunchuk Bridge...");

  setupNunchuk();
  setupEspNowBridge();
  setupBleGateway();

  Serial.println("Master Nunchuk Bridge Ready & Running!");
}

void loop() {
  readNunchukAndProcess();
  delay(15); // ~60Hz polling rate
}
