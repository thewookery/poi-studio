// Sub-Modules
#include "open_pixel_poi_led.cpp"
#include "open_pixel_poi_ble.cpp"
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
OpenPixelPoiBLE ble(config);
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
  ble.setup();
  button.setup();
  debugf("- Setup Complete. Running Free Heap: %d bytes\n", ESP.getFreeHeap());
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
      // jammed
      if(millis() - ble.bleLastReceived > 5000){
        ble.multipartPattern = 0;
      }
    }
  }
}






