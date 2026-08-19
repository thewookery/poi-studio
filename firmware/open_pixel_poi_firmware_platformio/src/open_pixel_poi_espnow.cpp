#ifndef _OPEN_PIXEL_POI_ESPNOW
#define _OPEN_PIXEL_POI_ESPNOW

#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "open_pixel_poi_config.cpp"

#define POI_SYNC_MAGIC 0x504F4953 // "POIS" (Open Pixel Poi Sync)

struct __attribute__((packed)) PoiSyncPacket {
  uint32_t magic;         // 0x504F4953
  uint8_t  senderId;      // Node ID to prevent echo loops
  uint8_t  cmd;           // 1 = State Sync
  uint8_t  patternBank;   // 0..4
  uint8_t  patternSlot;   // 0..9
  uint8_t  paletteFxMode; // 1..32 (0 = solid RGB)
  uint8_t  motionFxMode;  // 0..10
  uint8_t  brightness;    // 1..100
  uint16_t speed;         // Animation speed (e.g. 100 Hz / ms)
  uint8_t  displayState;  // DS_PATTERN, DS_BANK, etc.
  uint32_t timestampMs;   // Time alignment
};

class OpenPixelPoiEspNow {
private:
  OpenPixelPoiConfig& config;
  uint8_t myNodeId;
  uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool initialized = false;
  unsigned long lastBroadcastTime = 0;

public:
  inline static OpenPixelPoiEspNow* instance = nullptr;

  static void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (instance && len == sizeof(PoiSyncPacket)) {
      instance->handleIncomingPacket((const PoiSyncPacket*)incomingData);
    }
  }

  OpenPixelPoiEspNow(OpenPixelPoiConfig& _config): config(_config) {
    instance = this;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    myNodeId = mac[5] ^ mac[4]; // Unique hash
  }

  void setup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    esp_wifi_set_promiscuous(true);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
    esp_wifi_set_promiscuous(false);

    if (esp_now_init() != ESP_OK) {
      return;
    }

    initialized = true;
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 1;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_now_add_peer(&peerInfo);
  }

  void broadcastState(uint8_t cmd = 1) {
    if (!initialized) return;

    PoiSyncPacket pkt;
    pkt.magic = POI_SYNC_MAGIC;
    pkt.senderId = myNodeId;
    pkt.cmd = cmd;
    pkt.patternBank = config.patternBank;
    pkt.patternSlot = config.patternSlot;
    pkt.paletteFxMode = config.paletteFxMode;
    pkt.motionFxMode = config.motionFxMode;
    pkt.brightness = config.ledBrightness;
    pkt.speed = config.animationSpeed;
    pkt.displayState = (uint8_t)config.displayState;
    pkt.timestampMs = millis();

    esp_now_send(broadcastAddress, (uint8_t*)&pkt, sizeof(pkt));
    lastBroadcastTime = millis();
  }

  void handleIncomingPacket(const PoiSyncPacket* pkt) {
    if (pkt->magic != POI_SYNC_MAGIC) return;
    if (pkt->senderId == myNodeId) return;

    bool bankChanged = (config.patternBank != pkt->patternBank);
    bool slotChanged = (config.patternSlot != pkt->patternSlot);

    config.paletteFxMode = pkt->paletteFxMode;
    config.motionFxMode = pkt->motionFxMode;

    if (pkt->brightness > 0 && config.ledBrightness != pkt->brightness) {
      config.setLedBrightness(pkt->brightness);
    }
    if (pkt->speed > 0 && config.animationSpeed != pkt->speed) {
      config.setAnimationSpeed(pkt->speed);
    }

    if (bankChanged || slotChanged) {
      config.patternBank = pkt->patternBank;
      config.patternSlot = pkt->patternSlot;
      config.setPatternSlot(pkt->patternSlot, true);
    }

    if (pkt->displayState != (uint8_t)config.displayState) {
      config.displayState = (DisplayState)pkt->displayState;
      config.displayStateLastUpdated = millis();
    }
  }
};

#endif
