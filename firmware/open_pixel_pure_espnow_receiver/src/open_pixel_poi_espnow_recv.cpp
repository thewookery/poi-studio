#ifndef _OPEN_PIXEL_POI_ESPNOW_RECV
#define _OPEN_PIXEL_POI_ESPNOW_RECV

#include <esp_wifi.h>
#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <FS.h>
#include <LittleFS.h>
#include "open_pixel_poi_config.cpp"

#define ESPNOW_PACKET_MAGIC 0xA5
#define ESPNOW_DATA_MAGIC   0xA6

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

static OpenPixelPoiConfig* g_espnow_config = nullptr;
static uint8_t g_uploadSlot = 0;
static uint8_t g_uploadHeight = 55;
static uint16_t g_uploadWidth = 30;
static size_t g_uploadOffset = 0;
static bool g_isUploading = false;

static void onEspNowDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (g_espnow_config == nullptr) return;

  // 1. Handle Bulk Pattern Data Chunks (Magic 0xA6)
  if (len == sizeof(EspNowDataPacket)) {
    const EspNowDataPacket *dPkt = (const EspNowDataPacket *)incomingData;
    if (dPkt->magic == ESPNOW_DATA_MAGIC && dPkt->cmd == CMD_PATTERN_DATA_CHUNK) {
      if (g_isUploading) {
        if (g_uploadOffset + dPkt->dataLen <= PATTERN_PIXEL_LIMIT * 3) {
          memcpy(g_espnow_config->pattern + g_uploadOffset, dPkt->data, dPkt->dataLen);
          g_uploadOffset += dPkt->dataLen;
        }
      }
      return;
    }
  }

  // 2. Handle Control Packets (Magic 0xA5)
  if (len != sizeof(EspNowPacket)) return;
  const EspNowPacket *pkt = (const EspNowPacket *)incomingData;
  if (pkt->magic != ESPNOW_PACKET_MAGIC) return;

  g_espnow_config->displayState = DS_PATTERN;
  g_espnow_config->displayStateLastUpdated = millis();

  switch (pkt->cmd) {
    case CMD_SET_PATTERN: {
      uint8_t slot = pkt->val1 % PATTERN_BANK_SIZE;
      g_espnow_config->setPatternSlot(slot, false);
      Serial.printf("[ESP-NOW] Switched to Slot %d\n", slot);
      break;
    }
    case CMD_SET_BANK: {
      uint8_t bank = pkt->val1 % PATTERN_BANK_COUNT;
      g_espnow_config->setPatternBank(bank, false);
      Serial.printf("[ESP-NOW] Switched to Bank %d\n", bank);
      break;
    }
    case CMD_SET_PALETTE: {
      uint8_t pal = pkt->val1 % 33;
      g_espnow_config->setPaletteFxMode(pal);
      Serial.printf("[ESP-NOW] Switched to Palette %d\n", pal);
      break;
    }
    case CMD_SET_MOTION_FX: {
      uint8_t mot = pkt->val1 % 17;
      g_espnow_config->setMotionFxMode(mot);
      Serial.printf("[ESP-NOW] Switched to Motion FX %d\n", mot);
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
        g_espnow_config->setMotionFxMode(15);
      } else {
        g_espnow_config->setMotionFxMode(0);
      }
      break;
    }
    case CMD_SET_BRIGHTNESS: {
      g_espnow_config->setLedBrightness(pkt->val1);
      break;
    }
    case CMD_START_PATTERN_UPLOAD: {
      g_uploadSlot = pkt->val1;
      g_uploadHeight = (pkt->val2 > 0) ? pkt->val2 : 55;
      g_uploadWidth = (pkt->tiltX > 0) ? (uint16_t)pkt->tiltX : 30;
      g_uploadOffset = 0;

      uint8_t targetBank = g_uploadSlot / PATTERN_BANK_SIZE;
      uint8_t targetSlot = g_uploadSlot % PATTERN_BANK_SIZE;
      g_espnow_config->patternBank = targetBank;
      g_espnow_config->patternSlot = targetSlot;
      g_espnow_config->frameHeight = g_uploadHeight;
      g_espnow_config->frameCount = g_uploadWidth;
      g_espnow_config->setFrameHeight(g_uploadHeight);
      g_espnow_config->setFrameCount(g_uploadWidth);

      g_isUploading = true;
      Serial.printf("[ESP-NOW OTA] Started upload for Bank %d Slot %d (%dx%d)...\n",
                    targetBank, targetSlot, g_uploadWidth, g_uploadHeight);
      break;
    }
    case CMD_END_PATTERN_UPLOAD: {
      if (g_isUploading) {
        g_isUploading = false;
        
        uint8_t targetBank = g_uploadSlot / PATTERN_BANK_SIZE;
        uint8_t targetSlot = g_uploadSlot % PATTERN_BANK_SIZE;
        g_espnow_config->patternBank = targetBank;
        g_espnow_config->patternSlot = targetSlot;
        g_espnow_config->frameHeight = g_uploadHeight;
        g_espnow_config->frameCount = g_uploadWidth;
        g_espnow_config->setFrameHeight(g_uploadHeight);
        g_espnow_config->setFrameCount(g_uploadWidth);

        if (g_uploadOffset > 0) {
          g_espnow_config->patternLength = g_uploadOffset;

          // Commit RAM buffer to LittleFS
          String path = String("/pattern") + g_uploadSlot + ".oppp";
          File file = LittleFS.open(path, FILE_WRITE);
          if (file) {
            file.write(g_espnow_config->pattern, g_uploadOffset);
            file.flush();
            file.close();
          }
        }

        g_espnow_config->displayState = DS_PATTERN;
        g_espnow_config->displayStateLastUpdated = millis();
        Serial.printf("✅ [ESP-NOW OTA] Upload COMPLETE & Active: Bank %d Slot %d (%d bytes, %dx%d)!\n",
                      targetBank, targetSlot, g_uploadOffset, g_uploadWidth, g_uploadHeight);
      }
      break;
    }
    default:
      break;
  }
}

class OpenPixelPoiEspNowRecv {
  private:
    OpenPixelPoiConfig* pConfig;
  public:
    OpenPixelPoiEspNowRecv(OpenPixelPoiConfig& config) {
      pConfig = &config;
      g_espnow_config = &config;
    }

    void setup() {
      g_espnow_config = pConfig;
      WiFi.mode(WIFI_STA);
      WiFi.disconnect();

      esp_wifi_set_promiscuous(true);
      esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
      esp_wifi_set_promiscuous(false);

      if (esp_now_init() != ESP_OK) {
        Serial.println("ESP-NOW Init Failed!");
        return;
      }

      esp_now_register_recv_cb(onEspNowDataRecv);
      Serial.println("Pure ESP-NOW Receiver Ready on Channel 1!");
    }

    void loop() {
    }
};

#endif // _OPEN_PIXEL_POI_ESPNOW_RECV
