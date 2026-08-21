#ifndef _OPEN_PIXEL_POI_ESPNOW_RECV
#define _OPEN_PIXEL_POI_ESPNOW_RECV

#include <Arduino.h>
#include <esp_now.h>
#include <WiFi.h>
#include "config.h"
#include "open_pixel_poi_config.cpp"

#define ESPNOW_PACKET_MAGIC 0xA5

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

static OpenPixelPoiConfig* g_espnow_config = nullptr;

static void onEspNowDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (g_espnow_config == nullptr) return;
  if (len != sizeof(EspNowPacket)) return;
  const EspNowPacket *pkt = (const EspNowPacket *)incomingData;
  if (pkt->magic != ESPNOW_PACKET_MAGIC) return;

  switch (pkt->cmd) {
    case CMD_SET_PATTERN: {
      uint8_t slot = pkt->val1;
      if (slot < PATTERN_BANK_SIZE) {
        g_espnow_config->setPatternSlot(slot, false);
      }
      break;
    }
    case CMD_SET_BANK: {
      uint8_t bank = pkt->val1;
      if (bank < PATTERN_BANK_COUNT) {
        g_espnow_config->setPatternBank(bank, false);
      }
      break;
    }
    case CMD_SET_PALETTE: {
      g_espnow_config->setPaletteFxMode(pkt->val1 % 33);
      break;
    }
    case CMD_SET_MOTION_FX: {
      g_espnow_config->setMotionFxMode(pkt->val1 % 17);
      break;
    }
    case CMD_SET_SPEED: {
      uint16_t spd = ((uint16_t)pkt->val1 << 8) | pkt->val2;
      if (spd >= 1 && spd <= 2000) {
        g_espnow_config->setAnimationSpeed(spd);
      }
      break;
    }
    case CMD_STROBE_BLAST: {
      if (pkt->val1 == 1) {
        g_espnow_config->setMotionFxMode(15); // Disco Strobe Blast on hold
      } else {
        g_espnow_config->setMotionFxMode(0);  // Return to normal
      }
      break;
    }
    case CMD_SET_BRIGHTNESS: {
      g_espnow_config->setLedBrightness(pkt->val1);
      break;
    }
    default:
      break;
  }
}

class OpenPixelPoiEspNowRecv {
  public:
    OpenPixelPoiConfig& config;

    OpenPixelPoiEspNowRecv(OpenPixelPoiConfig& configRef) : config(configRef) {
      g_espnow_config = &configRef;
    }

    void setup() {
      g_espnow_config = &config;
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();

      if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed!");
        return;
      }

      esp_now_register_recv_cb(onEspNowDataRecv);
      Serial.println("Pure ESP-NOW Receiver Active on Channel 1!");
    }

    void loop() {
      // Async
    }
};

#endif
