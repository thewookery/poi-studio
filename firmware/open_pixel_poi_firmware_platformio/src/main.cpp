// Sub-Modules
#ifdef USE_WIFI_MODE
#include "open_pixel_poi_wifi.cpp"
#else
#include "open_pixel_poi_ble.cpp"
#endif
#include "open_pixel_poi_led.cpp"
#include "open_pixel_poi_button.cpp"

//#define DEBUG  // Comment this line out to remove printf statements in released version
#ifdef DEBUG
#define debugf(...) Serial.print("<<main>> ");Serial.printf(__VA_ARGS__);
#define debugf_noprefix(...) Serial.printf(__VA_ARGS__);
#else
#define debugf(...)
#define debugf_noprefix(...)
#endif


OpenPixelPoiConfig config;
#ifdef USE_WIFI_MODE
OpenPixelPoiWiFi wifi(config);
#else
OpenPixelPoiBLE ble(config);
#endif
OpenPixelPoiLED led(config);
OpenPixelPoiButton button(config);



void setup() {
  #ifdef DEBUG
    Serial.begin(19200);
    Serial.setDebugOutput(true);
  #endif

  debugf("Open Pixel POI - Hardened Engine\n");
  debugf("Setup Begin. Free Heap: %d bytes\n", ESP.getFreeHeap());

  // MEMORY INTEGRITY GUARDRAIL:
  // Ensure pattern buffer exists; if initial heap was constrained, allocate safe fallback
  if (config.pattern == NULL) {
    config.pattern = (uint8_t *) malloc(10000 * 3 * sizeof(uint8_t));
  }

  config.setup();
  led.setup();
#ifdef USE_WIFI_MODE
  wifi.setup();
#else
  ble.setup();
#endif
  button.setup();
  debugf("- Setup Complete. Running Free Heap: %d bytes\n", ESP.getFreeHeap());
}

void loop() {
  while(true){
#ifdef USE_WIFI_MODE
    wifi.loop();
    config.loop();
    led.loop();
    button.loop();
#else
    if(ble.multipartPattern == 0){
      ble.loop();
      config.loop();
      led.loop();
      button.loop();
    }else{
      delay(250);
      // jammed
      if(millis() - ble.bleLastReceived > 5000){
        ble.multipartPattern = 0;
      }
    }
#endif
  }
}




