#ifndef _OPEN_PIXEL_POI_WIFI
#define _OPEN_PIXEL_POI_WIFI

#ifdef USE_WIFI_MODE

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "open_pixel_poi_config.cpp"

#define POI_SYNC_MAGIC 0x504F4953 // "POIS"

struct __attribute__((packed)) PoiSyncPacket {
  uint32_t magic;
  uint8_t  senderId;
  uint8_t  patternBank;
  uint8_t  patternSlot;
  uint8_t  paletteFxMode;
  uint8_t  motionFxMode;
  uint8_t  brightness;
  uint16_t speed;
  uint32_t timestampMs;
};

class OpenPixelPoiWiFi {
private:
  OpenPixelPoiConfig& config;
  WebServer server;
  uint8_t myNodeId;
  uint8_t broadcastAddress[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  bool espNowInitialized = false;

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

  void setCorsHeaders() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "*");
  }

public:
  inline static OpenPixelPoiWiFi* instance = nullptr;

  OpenPixelPoiWiFi(OpenPixelPoiConfig& _config): config(_config), server(80) {
    instance = this;
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    myNodeId = mac[5] ^ mac[4];
  }

  void setup() {
    // 1. Start SoftAP on Channel 1 (Zero-conflict dedicated channel)
    WiFi.mode(WIFI_AP_STA);
    WiFi.softAP("OpenPixelPoi", "openpoi123", 1, 0, 4);

    // 2. Initialize ESP-NOW on the active Wi-Fi channel
    if (esp_now_init() == ESP_OK) {
      espNowInitialized = true;
      esp_now_register_recv_cb(onDataRecv);

      esp_now_peer_info_t peerInfo = {};
      memcpy(peerInfo.peer_addr, broadcastAddress, 6);
      peerInfo.channel = 1;
      peerInfo.encrypt = false;
      peerInfo.ifidx = WIFI_IF_AP;
      esp_now_add_peer(&peerInfo);
    }

    // 3. Register HTTP Routes with CORS
    server.on("/api/status", HTTP_OPTIONS, [this]() {
      setCorsHeaders();
      server.send(204);
    });

    server.on("/api/status", HTTP_GET, [this]() {
      setCorsHeaders();
      String json = "{";
      json += "\"bank\":" + String(config.patternBank) + ",";
      json += "\"slot\":" + String(config.patternSlot) + ",";
      json += "\"palette\":" + String(config.paletteFxMode) + ",";
      json += "\"motion\":" + String(config.motionFxMode) + ",";
      json += "\"brightness\":" + String(config.ledBrightness) + ",";
      json += "\"speed\":" + String(config.animationSpeed) + ",";
      json += "\"freeBytes\":" + String(LittleFS.totalBytes() - LittleFS.usedBytes()) + ",";
      json += "\"totalBytes\":" + String(LittleFS.totalBytes()) + ",";
      json += "\"voltage\":" + String(config.batteryVoltage, 2);
      json += "}";
      server.send(200, "application/json", json);
    });

    server.on("/api/state", HTTP_OPTIONS, [this]() {
      setCorsHeaders();
      server.send(204);
    });

    server.on("/api/state", HTTP_POST, [this]() {
      setCorsHeaders();
      if (server.hasArg("bank")) {
        uint8_t b = server.arg("bank").toInt();
        config.setPatternBank(b % PATTERN_BANK_COUNT, true);
      }
      if (server.hasArg("slot")) {
        uint8_t s = server.arg("slot").toInt();
        config.setPatternSlot(s % PATTERN_BANK_SIZE, true);
      }
      if (server.hasArg("palette")) {
        uint8_t p = server.arg("palette").toInt();
        config.setPaletteFxMode(p);
      }
      if (server.hasArg("motion")) {
        uint8_t m = server.arg("motion").toInt();
        config.setMotionFxMode(m);
      }
      if (server.hasArg("brightness")) {
        uint8_t br = server.arg("brightness").toInt();
        config.setLedBrightness(br);
      }
      if (server.hasArg("speed")) {
        uint16_t sp = server.arg("speed").toInt();
        config.setAnimationSpeed(sp);
      }

      config.displayState = DS_PATTERN;
      config.displayStateLastUpdated = millis();
      broadcastState();

      server.send(200, "application/json", "{\"success\":true}");
    });

    server.on("/api/pattern", HTTP_OPTIONS, [this]() {
      setCorsHeaders();
      server.send(204);
    });

    server.on("/api/pattern", HTTP_POST, [this]() {
      setCorsHeaders();
      server.send(200, "application/json", "{\"success\":true}");
    }, [this]() {
      // Streamed pattern upload handler
      HTTPUpload& upload = server.upload();
      static File patternFile;
      static uint8_t targetBank = 0;
      static uint8_t targetSlot = 0;
      static uint8_t targetHeight = 55;
      static uint16_t targetWidth = 40;

      if (upload.status == UPLOAD_FILE_START) {
        if (server.hasArg("bank")) targetBank = server.arg("bank").toInt();
        if (server.hasArg("slot")) targetSlot = server.arg("slot").toInt();
        if (server.hasArg("height")) targetHeight = server.arg("height").toInt();
        if (server.hasArg("width")) targetWidth = server.arg("width").toInt();

        config.setPatternBank(targetBank % PATTERN_BANK_COUNT, true);
        config.setPatternSlot(targetSlot % PATTERN_BANK_SIZE, true);
        config.setFrameHeight(targetHeight);
        config.setFrameCount(targetWidth);

        String path = "/p";
        path += config.getActivePatternIndex();
        path += ".bmp";
        patternFile = LittleFS.open(path, "w");
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (patternFile) {
          patternFile.write(upload.buf, upload.currentSize);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (patternFile) {
          patternFile.close();
        }
        config.patternLength = config.frameHeight * config.frameCount * 3;
        config.loadFrameHeight();
        config.loadFrameCount();
        config.startLoadingPattern();
        broadcastState();
      }
    });

    server.on("/", HTTP_GET, [this]() {
      setCorsHeaders();
      String html = "<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width,initial-scale=1'><title>Open Pixel Poi</title>";
      html += "<style>body{background:#0a0a0f;color:#00ff88;font-family:sans-serif;text-align:center;padding:40px 20px;}";
      html += ".card{background:rgba(255,255,255,0.05);border:1px solid rgba(0,255,136,0.3);border-radius:12px;padding:24px;max-width:400px;margin:0 auto;}";
      html += "a{display:inline-block;margin-top:20px;padding:12px 24px;background:#00ff88;color:#000;text-decoration:none;font-weight:bold;border-radius:6px;}</style></head>";
      html += "<body><div class='card'><h2>⚡ Open Pixel Poi</h2><p>Connected to Poi Wi-Fi Network!</p>";
      html += "<p>Bank: <b>" + String(config.patternBank + 1) + "</b> • Slot: <b>" + String(config.patternSlot + 1) + "</b></p>";
      html += "<a href='https://thewookery.github.io/poi-studio/'>Open Studio App</a></div></body></html>";
      server.send(200, "text/html", html);
    });

    server.begin();
  }

  void loop() {
    server.handleClient();

    if (hasPendingSync) {
      hasPendingSync = false;
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
  }

  void broadcastState() {
    if (!espNowInitialized) return;

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

#endif // USE_WIFI_MODE
#endif // _OPEN_PIXEL_POI_WIFI
