#include "NeoPixelStrip.h"
NeoPixelStrip::NeoPixelStrip(uint16_t count, uint8_t dataPin) : strip(count, dataPin) {}

void NeoPixelStrip::Begin() { strip.Begin(); }
void NeoPixelStrip::Show() { strip.Show(); }
void NeoPixelStrip::SetPixelColor(uint16_t i, RgbColor color) { strip.SetPixelColor(i, color); }
void NeoPixelStrip::ClearTo(RgbColor color) { strip.ClearTo(color); }
void NeoPixelStrip::SetBrightness(uint8_t i) { 
#ifdef PEBBLE_50PX
    // Micro Pebble Seed Pixels draw only ~12mA max per pixel (not 150mA 5050 power LEDs)
    // Scale 1-100% brightness directly to 0-255 luminance!
    if (i <= 1) {
        strip.SetLuminance(i);
    } else {
        uint8_t lum = (uint8_t)((i * 255) / 100);
        strip.SetLuminance(lum);
    }
#else
    strip.SetLuminance(
        CalculateLuminance(i, strip.PixelCount(), OUTPUT_WS2812B_5050_DRAW, OUTPUT_WS2812B_5050_LIMIT)
    ); 
#endif
}
uint8_t NeoPixelStrip::GetLuminance() { return strip.GetLuminance(); }
