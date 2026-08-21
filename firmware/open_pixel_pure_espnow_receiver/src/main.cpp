#include "open_pixel_poi_led.cpp"
#include "open_pixel_poi_espnow_recv.cpp"
#include "open_pixel_poi_button.cpp"

OpenPixelPoiConfig config;
OpenPixelPoiLED led(config);
OpenPixelPoiEspNowRecv espnow(config);
OpenPixelPoiButton button(config);

void setup() {
  Serial.begin(115200);
  delay(100);

  if (config.pattern == NULL) {
    config.pattern = (uint8_t *) malloc(10000 * 3 * sizeof(uint8_t));
  }

  config.setup();
  led.setup();
  espnow.setup();
  button.setup();

  Serial.println("Pure ESP-NOW Receiver Setup Complete!");
}

void loop() {
  while(true) {
    config.loop();
    led.loop();
    espnow.loop();
    button.loop();
  }
}
