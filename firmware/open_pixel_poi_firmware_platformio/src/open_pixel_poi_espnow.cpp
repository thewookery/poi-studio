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
  uint8_t  patternBank;   // 0..4
  uint8_t  patternSlot;   // 0..9
  uint8_t  paletteFxMode; // 1..32 (0 = solid RGB)
  uint8_t  motionFxMode;  // 0..10
  uint8_t  brightness;    // 1..100
  uint16_t speed;         // Animation speed (e.g. 100 Hz / ms)
  uint32_t timestampMs;   // Time alignment
};

class OpenPixelPoiEspNow {
private:
  OpenPixelPoiConfig& config;
  uint8_t myNodeId;
  uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool initialized = false;

  volatile bool hasPendingSync = false;
  PoiSyncPacket pendingPacket;

  static void onDataRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
    if (instance && len == sizeof(PoiSyncPacket)) {
      const PoiSyncPacket* pkt = (const PoiSyncPacket*)incomingData;
      if (pkt->magic == POI_SYNC_MAGIC && pkt->senderId != instance->myNodeId) {
        instance->pendingPacket = *pkt;
        instance->hasPendingSync = true;
      }
    }
  }

public:
  inline static OpenPixelPoiEspNow* instance = nullptr;

  OpenPixelPoiEspNow(OpenPixelPoiConfig& _config): config(_config) {
    instance = this;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    myNodeId = mac[5] ^ mac[4]; // Unique hash
  }

  void setup() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() != ESP_OK) {
      return;
    }

    initialized = true;
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, broadcastAddress, 6);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;

    esp_now_add_peer(&peerInfo);
  }

  void loop() {
    if (!hasPendingSync) return;
    hasPendingSync = false;

    // Safely execute on main loop (NOT inside hardware interrupt ISR!)
    bool bankChanged = (config.patternBank != pendingPacket.patternBank);
    bool slotChanged = (config.patternSlot != pendingPacket.patternSlot);

    if (bankChanged) {
      config.setPatternBank(pendingPacket.patternBank, false);
    }
    if (slotChanged || bankChanged) {
      config.setPatternSlot(pendingPacket.patternSlot, false);
    }

    config.paletteFxMode = pendingPacket.paletteFxMode;
    config.motionFxMode = pendingPacket.motionFxMode;

    if (pendingPacket.brightness > 0 && config.ledBrightness != pendingPacket.brightness) {
      config.setLedBrightness(pendingPacket.brightness);
    }
    if (pendingPacket.speed > 0 && config.animationSpeed != pendingPacket.speed) {
      config.setAnimationSpeed(pendingPacket.speed);
    }

    config.displayState = DS_PATTERN;
    config.displayStateLastUpdated = millis();
  }

  void broadcastState() {
    if (!initialized) return;

    PoiSyncPacket pkt;
    pkt.magic = POI_SYNC_MAGIC;
    pkt.senderId = myNodeId;
    pkt.patternBank = config.patternBank;
    pkt.patternSlot = config.patternSlot;
    pkt.paletteFxMode = config.paletteFxMode;
    pkt.motionFxMode = config.motionFxMode;
    pkt.brightness = config.ledBrightness;
    pkt.speed = config.animationSpeed;
    pkt.timestampMs = millis();

    esp_now_send(broadcastAddress, (uint8_t*)&pkt, sizeof(pkt));
  }
};

#endif
