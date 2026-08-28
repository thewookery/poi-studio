// ============================================================================
// OPEN PIXEL POI - HYBRID DUAL-ENGINE (Nordic BLE Flashing + ESP-NOW Nunchuk)
// ============================================================================

#include "open_pixel_poi_led.cpp"
#include "open_pixel_poi_ble.cpp"
#include "open_pixel_poi_button.cpp"
#include <esp_wifi.h>
#include <WiFi.h>
#include <esp_now.h>

#define ESPNOW_PACKET_MAGIC 0xA5

enum EspNowCommand {
  CMD_NONE = 0,
  CMD_SET_PATTERN = 1,
  CMD_SET_BANK = 2,
  CMD_SET_PALETTE = 3,
  CMD_SET_MOTION_FX = 4,
  CMD_SET_SPEED = 5,
  CMD_STROBE_BLAST = 6,
  CMD_SET_BRIGHTNESS = 7,
  CMD_TILT_MODULATION = 8
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

OpenPixelPoiConfig config;
OpenPixelPoiBLE ble(config);
OpenPixelPoiLED led(config);
OpenPixelPoiButton button(config);

static void onEspNowNunchukRecv(const uint8_t *mac, const uint8_t *incomingData, int len) {
  if (len != sizeof(EspNowPacket)) return;
  const EspNowPacket *pkt = (const EspNowPacket *)incomingData;
  if (pkt->magic != ESPNOW_PACKET_MAGIC) return;

  config.displayState = DS_PATTERN;
  config.displayStateLastUpdated = millis();

  switch (pkt->cmd) {
    case CMD_SET_PATTERN: {
      uint8_t slot = pkt->val1 % PATTERN_BANK_SIZE;
      config.setPatternSlot(slot, false);
      break;
    }
    case CMD_SET_BANK: {
      uint8_t bank = pkt->val1 % PATTERN_BANK_COUNT;
      config.setPatternBank(bank, false);
      break;
    }
    case CMD_SET_PALETTE: {
      uint8_t pal = pkt->val1 % 33;
      config.setPaletteFxMode(pal);
      break;
    }
    case CMD_SET_MOTION_FX: {
      uint8_t mot = pkt->val1 % 17;
      config.setMotionFxMode(mot);
      break;
    }
    case CMD_SET_SPEED: {
      uint16_t spd = ((uint16_t)pkt->val1 << 8) | pkt->val2;
      if (spd >= 1 && spd <= 2000) {
        config.setAnimationSpeed(spd);
      }
      break;
    }
    case CMD_STROBE_BLAST: {
      if (pkt->val1 == 1) {
        config.setMotionFxMode(15); // Strobe mode
      } else {
        config.setMotionFxMode(0);
      }
      break;
    }
    case CMD_SET_BRIGHTNESS: {
      config.setLedBrightness(pkt->val1);
      break;
    }
    default:
      break;
  }
}

void setupEspNowListener() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onEspNowNunchukRecv);
  }
}

void setup() {
  // Allocate pattern buffer if unallocated
  if (config.pattern == NULL) {
    config.pattern = (uint8_t *) malloc(10000 * 3 * sizeof(uint8_t));
  }

  config.setup();
  led.setup();
  ble.setup();
  button.setup();
  setupEspNowListener();
}

void loop() {
  while(true){
    if(ble.multipartPattern == 0){
      ble.loop();
      config.loop();
      led.loop();
      button.loop();
    }else{
      delay(250);
      if(millis() - ble.bleLastReceived > 5000){
        ble.multipartPattern = 0;
      }
    }
  }
}
