#ifndef _OPEN_PIXEL_POI_MESH
#define _OPEN_PIXEL_POI_MESH

#include <esp_now.h>
#include <WiFi.h>
#include "open_pixel_poi_config.cpp"

// Sync packet structure (16 bytes)
typedef struct __attribute__((packed)) {
  uint8_t magic;           // 0xAA
  uint8_t senderId;        // Low byte of MAC to avoid self-echo
  uint8_t bank;            // 0-3
  uint8_t slot;            // 0-4
  uint8_t palette;         // 0-24
  uint8_t blend;           // 0-4
  uint8_t brightness;      // 1-100
  uint16_t speed;          // FPS
  uint32_t timestampMs;    // Frame alignment
} PoiMeshPacket;

static uint8_t s_localMeshId = 0;
static OpenPixelPoiConfig* s_pConfig = nullptr;
static uint8_t s_broadcastMac[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

// Receive callback
static void onMeshRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(PoiMeshPacket) || s_pConfig == nullptr) return;
  PoiMeshPacket pkt;
  memcpy(&pkt, incomingData, sizeof(PoiMeshPacket));
  if (pkt.magic != 0xAA || pkt.senderId == s_localMeshId) return; // Ignore own echoes

  // Apply synced state to local Poi
  if (pkt.bank != s_pConfig->patternBank) {
    s_pConfig->setPatternBank(pkt.bank, false);
  }
  if (pkt.slot != s_pConfig->patternSlot) {
    s_pConfig->setPatternSlot(pkt.slot, false);
  }
  if (pkt.palette != s_pConfig->paletteFxMode) {
    s_pConfig->setPaletteFxMode(pkt.palette);
  }
  if (pkt.blend != s_pConfig->blendMode) {
    s_pConfig->setBlendMode(pkt.blend);
  }
  if (pkt.brightness != s_pConfig->ledBrightness) {
    s_pConfig->setLedBrightness(pkt.brightness);
  }
  if (pkt.speed != s_pConfig->animationSpeed) {
    s_pConfig->setAnimationSpeed(pkt.speed);
  }
}

class OpenPixelPoiMesh {
private:
  OpenPixelPoiConfig& config;
  bool isInitialized = false;

public:
  OpenPixelPoiMesh(OpenPixelPoiConfig& _config) : config(_config) {
    s_pConfig = &_config;
  }

  void setup() {
    uint64_t mac = ESP.getEfuseMac();
    s_localMeshId = (uint8_t)(mac & 0xFF);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    if (esp_now_init() == ESP_OK) {
      isInitialized = true;
      esp_now_register_recv_cb(onMeshRecv);

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, s_broadcastMac, 6);
      peerInfo.channel = 1;
      peerInfo.encrypt = false;
      esp_now_add_peer(&peerInfo);
    }
  }

  void broadcastState() {
    if (!isInitialized) return;
    PoiMeshPacket pkt;
    pkt.magic = 0xAA;
    pkt.senderId = s_localMeshId;
    pkt.bank = config.patternBank;
    pkt.slot = config.patternSlot;
    pkt.palette = config.paletteFxMode;
    pkt.blend = config.blendMode;
    pkt.brightness = config.ledBrightness;
    pkt.speed = config.animationSpeed;
    pkt.timestampMs = millis();

    esp_now_send(s_broadcastMac, (uint8_t *)&pkt, sizeof(PoiMeshPacket));
  }

  void loop() {
    // Zero-overhead passive listener
  }
};

#endif
