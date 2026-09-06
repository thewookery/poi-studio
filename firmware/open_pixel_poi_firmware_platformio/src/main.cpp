// ============================================================================
// OPEN PIXEL POI - PURE NORDIC BLE SLIM EDITION (Build v179)
// Zero Wi-Fi bloat • Instant 20MHz SPI DotStar timing • Pure Nordic UART BLE
// ============================================================================

#include "open_pixel_poi_led.cpp"
#include "open_pixel_poi_ble.cpp"
#include "open_pixel_poi_button.cpp"

OpenPixelPoiConfig config;
OpenPixelPoiBLE ble(config);
OpenPixelPoiLED led(config);
OpenPixelPoiButton button(config);

void setup() {
  // Allocate pattern buffer if unallocated
  if (config.pattern == NULL) {
    config.pattern = (uint8_t *) malloc(PATTERN_PIXEL_LIMIT * 3 * sizeof(uint8_t));
  }

  config.setup();
  led.setup();
  ble.setup();
  button.setup();
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
