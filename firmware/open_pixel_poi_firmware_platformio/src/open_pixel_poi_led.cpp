#ifndef _OPEN_PIXEL_POI_LED
#define _OPEN_PIXEL_POI_LED

#include "open_pixel_poi_config.cpp"
#include "led_strip/ILedStrip.h"
#include "led_strip/NoStrip.h"
#include "led_strip/NeoPixelStrip.h"
#include "led_strip/DotStarStrip.h"

#include <NeoPixelBusLg.h>
//#define DEBUG  // Comment this line out to remove printf statements in released version
#ifdef DEBUG
#define debugf(...) Serial.print("  <<led>> ");Serial.printf(__VA_ARGS__);
#define debugf_noprefix(...) Serial.printf(__VA_ARGS__);
#else
#define debugf(...)
#define debugf_noprefix(...)
#endif

// 25 Zero-RAM Real-Time Color Palettes
static inline void applyPaletteFX(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t mode, unsigned long timeMs, uint8_t speed) {
  if (mode == 0) return; // 0: Normal RGB

  uint8_t lum = (uint8_t)((red * 77 + green * 150 + blue * 29) >> 8);

  switch (mode) {
    case 1: { // 1. RAINBOW HUE CYCLE
      uint8_t hue = (uint8_t)((timeMs * speed / 25) & 0xFF);
      uint8_t region = hue / 43;
      uint8_t rem = (hue - (region * 43)) * 6;
      uint8_t p = (lum * 75) >> 8;
      uint8_t q = (lum * (255 - ((180 * rem) >> 8))) >> 8;
      uint8_t t = (lum * (255 - ((180 * (255 - rem)) >> 8))) >> 8;
      switch (region % 6) {
        case 0: red = lum; green = t; blue = p; break;
        case 1: red = q; green = lum; blue = p; break;
        case 2: red = p; green = lum; blue = t; break;
        case 3: red = p; green = q; blue = lum; break;
        case 4: red = t; green = p; blue = lum; break;
        default: red = lum; green = p; blue = q; break;
      }
      break;
    }
    case 2: // 2. CYBERPUNK NEON (Cyan & Magenta)
      red = lum; green = (uint8_t)((lum * 130) / 255); blue = (uint8_t)(255 - (lum * 90) / 255);
      break;
    case 3: // 3. FIRE & LAVA (Black -> Crimson -> Orange -> Gold -> White)
      if (lum < 85) { red = lum * 3; green = 0; blue = 0; }
      else if (lum < 170) { red = 255; green = (lum - 85) * 3; blue = 0; }
      else { red = 255; green = 255; blue = (lum - 170) * 3; }
      break;
    case 4: // 4. MATRIX PHOSPHOR GREEN
      if (lum > 220) { red = lum; green = 255; blue = lum; }
      else { red = (lum * 25) / 255; green = lum; blue = (lum * 35) / 255; }
      break;
    case 5: // 5. ACID VAPORWAVE
      red = 255 - red; green = (green > 128) ? 255 : (green * 2); blue = 255 - blue;
      break;
    case 6: // 6. GLACIAL ICE (Navy -> Aqua -> Diamond White)
      red = (lum * 130) / 255; green = (lum * 220) / 255; blue = (lum < 40) ? (lum * 6) : 255;
      break;
    case 7: // 7. SUNSET DUSK (Royal Purple -> Amber -> Warm Gold)
      if (lum < 128) { red = (lum * 180) / 128; green = 0; blue = 120 + (lum * 80) / 128; }
      else { red = 255; green = ((lum - 128) * 200) / 127; blue = 200 - ((lum - 128) * 150) / 127; }
      break;
    case 8: // 8. FOREST MOSS (Deep Pine Green -> Chartreuse Lime)
      red = (lum * 118) / 255; green = lum; blue = (lum * 20) / 255;
      break;
    case 9: // 9. ELECTRIC VIOLET (Deep Indigo -> Vivid Violet -> White)
      red = (lum * 200) / 255; green = (lum > 180) ? (lum - 180) * 3 : 0; blue = (lum < 100) ? lum * 2 : 255;
      break;
    case 10: // 10. VAPORWAVE PASTEL (Coral Pink -> Turquoise Mint)
      red = (uint8_t)(160 + (lum * 95) / 255); green = (uint8_t)(100 + (lum * 120) / 255); blue = (uint8_t)(255 - (lum * 60) / 255);
      break;
    case 11: // 11. MONOCHROME PHOSPHOR (High-Contrast Black & White)
      red = lum; green = lum; blue = lum;
      break;
    case 12: // 12. AMBER GOLD (Rich Honey -> Warm Gold)
      red = lum; green = (uint8_t)((lum * 180) / 255); blue = 0;
      break;
    case 13: // 13. DEEP OCEAN (Ultramarine -> Sea Teal)
      red = 0; green = (uint8_t)((lum * 200) / 255); blue = lum;
      break;
    case 14: // 14. MAGMA OBSIDIAN (Charcoal -> Ruby Crimson -> Molten Gold)
      if (lum < 100) { red = (lum * 200) / 100; green = 0; blue = 0; }
      else { red = 255; green = ((lum - 100) * 190) / 155; blue = ((lum - 100) * 30) / 155; }
      break;
    case 15: // 15. RADIOACTIVE TOXIC (Chartreuse & Acid Green)
      red = (uint8_t)((lum * 180) / 255); green = 255; blue = 0;
      break;
    case 16: // 16. COTTON CANDY (Bubblegum Pink & Sky Blue)
      red = (lum > 128) ? 255 : (lum * 2); green = (uint8_t)((lum * 100) / 255); blue = (lum < 128) ? 255 : (255 - (lum - 128) * 2);
      break;
    case 17: // 17. GALAXY NEBULA (Deep Indigo -> Nebula Magenta -> Starlight)
      red = (uint8_t)((lum * 224) / 255); green = (lum > 200) ? (lum - 200) * 4 : 0; blue = (uint8_t)(100 + (lum * 155) / 255);
      break;
    case 18: // 18. NEON LIME (Electric Yellow-Green)
      red = (uint8_t)((lum * 198) / 255); green = 255; blue = 0;
      break;
    case 19: // 19. PLASMA BEAM (Sapphire -> Electric Purple -> White)
      red = (uint8_t)((lum * 150) / 255); green = (lum > 210) ? (lum - 210) * 5 : 0; blue = lum;
      break;
    case 20: // 20. INFERNO SCARLET (Burgundy -> Flame Scarlet)
      red = lum; green = (uint8_t)((lum * 70) / 255); blue = (uint8_t)((lum * 20) / 255);
      break;
    case 21: // 21. BIOLUMINESCENCE (Abyssal Teal -> Seafoam Green)
      red = 0; green = lum; blue = (uint8_t)((lum * 170) / 255);
      break;
    case 22: // 22. CANDY APPLE (Cherry Red & Gloss White)
      red = lum; green = (lum > 180) ? (lum - 180) * 3 : 0; blue = (lum > 180) ? (lum - 180) * 3 : 0;
      break;
    case 23: // 23. GOLD & PLATINUM (Champagne Gold & Platinum)
      red = lum; green = (uint8_t)((lum * 210) / 255); blue = (uint8_t)((lum * 120) / 255);
      break;
    case 24: // 24. PSYCHEDELIC WARP (Negative Chroma Inversion)
      red = 255 - blue; green = 255 - red; blue = 255 - green;
      break;
  }
}

class OpenPixelPoiLED {

  private:
    OpenPixelPoiConfig& config;
    uint8_t red;
    uint8_t green;
    uint8_t blue;
    long lastFrameIndex = 0;

    // Declare our LED strip object:
    ILedStrip* ledStrip = new NoStrip();

  public:
    OpenPixelPoiLED(OpenPixelPoiConfig& _config): config(_config){}    
    int frameIndex;
    void setup(){
      debugf("Setup begin\n");
      // Create Led Strip objects based on config, default to DotStar 55px
      if(config.hardwareVersion == 1 && config.ledType == 1){
        ledStrip = new NeoPixelStrip(config.ledCount > 0 ? config.ledCount : 20, 8);
      }else if(config.hardwareVersion == 2 && config.ledType == 1){
        ledStrip = new NeoPixelStrip(config.ledCount > 0 ? config.ledCount : 25, 6);
      }else{
        // 55px DotStar (Pins 6 Data, 7 Clock)
        ledStrip = new DotStarStrip(config.ledCount > 0 ? config.ledCount : 55, 6, 7);
      }

      // LED Setup:
      ledStrip->Begin();
      frameIndex = 0;
      debugf("LED setup complete\n");
    }

    void loop(){
      // Set Brightness. 
      // Low voltage = force low brightness
      if(config.batteryState == BAT_LOW && config.ledBrightness > 10){
        ledStrip->SetBrightness(10);
      }else if(config.batteryState == BAT_CRITICAL || config.batteryState == BAT_SHUTDOWN){
        ledStrip->SetBrightness(1);
      }else{ // Normal operation
        ledStrip->SetBrightness(config.ledBrightness);
      }

      // Clear previous data
      ledStrip->ClearTo(RgbColor(0,0,0));

      // Render output with Pattern Blend Engine & Color Palette FX
      if(config.displayState == DS_PATTERN || config.displayState == DS_PATTERN_ALL  || config.displayState == DS_PATTERN_ALL_ALL){
        if(config.frameCount == 0 || config.frameHeight == 0) return;
        frameIndex = ((micros() - (config.displayStateLastUpdated * 1000)) / (1000000/(config.animationSpeed))) % config.frameCount;
        if(lastFrameIndex == frameIndex){
          return;
        }else{
          lastFrameIndex = frameIndex;
        }

        // Zero-RAM Transition Engine
        bool isTransitioning = (config.blendMode > 0 && (millis() - config.blendStartTime < config.blendDurationMs));
        float tProgress = isTransitioning ? ((float)(millis() - config.blendStartTime) / (float)config.blendDurationMs) : 1.0f;

        for (int j=0; j<config.ledCount; j++){
          red = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 0];
          green = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 1];
          blue = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 2];

          // Apply Zero-RAM Transition Modes
          if (isTransitioning) {
            if (config.blendMode == 1) {
              // 1. Smooth Fade-In (Linear ramp)
              red = (uint8_t)(red * tProgress);
              green = (uint8_t)(green * tProgress);
              blue = (uint8_t)(blue * tProgress);
            } else if (config.blendMode == 2) {
              // 2. Energy Flash Pulse
              if (tProgress < 0.25f) {
                uint8_t burst = (uint8_t)((1.0f - (tProgress / 0.25f)) * 255);
                red = min(255, (int)(red + burst));
                green = min(255, (int)(green + burst));
                blue = min(255, (int)(blue + burst));
              }
            } else if (config.blendMode == 3) {
              // 3. Curtain Wipe In (Wipes along strip)
              int wipeLimit = (int)(tProgress * config.ledCount);
              if (j > wipeLimit) {
                red = 0; green = 0; blue = 0;
              }
            } else if (config.blendMode == 4) {
              // 4. Glow Pulse In
              float pulse = 0.4f + 0.6f * tProgress;
              red = (uint8_t)(red * pulse);
              green = (uint8_t)(green * pulse);
              blue = (uint8_t)(blue * pulse);
            }
          }


          // Apply Real-Time Color Palette Filter (25 Zero-RAM Palettes)
          if (config.paletteFxMode > 0) {
            applyPaletteFX(red, green, blue, config.paletteFxMode, millis(), config.paletteSpeed);
          }

          ledStrip->SetPixelColor(config.ledCount-1-j, RgbColor(red, green, blue)); // Invert display for POV arc
        }
      }

else if(config.displayState == DS_WAITING || config.displayState == DS_WAITING2 || config.displayState == DS_WAITING3 || config.displayState == DS_WAITING4 || config.displayState == DS_WAITING5){
        // 500ms or till interupted
        if(config.displayState == DS_WAITING){
          // Blue for blinky!
          red = 0x00;
          green = 0x00;
          blue = 0xff;
        }else if(config.displayState == DS_WAITING2){
          // Pink for bank select & demo mode!
          red = 255;
          green = 0;
          blue = 255;
        }else if(config.displayState == DS_WAITING3){
          // White for brightness!
          red = 0x88;
          green = 0x88;
          blue = 0x88;
        }else if(config.displayState == DS_WAITING4){
          // RED for speed!
          red = 0xFF;
          green = 0x00;
          blue = 0x00;
        }else if(config.displayState == DS_WAITING5){
          // Green -> RED fade for battery!
          red = 0xFF * ((millis() - config.displayStateLastUpdated)/500.0);
          green = 0xFF - red;
          blue = 0x00;
        }
        for(int j=0; j<config.ledCount; j++){
          if(j == (millis() - config.displayStateLastUpdated)/(int)(500/(config.ledCount/2.0)) || config.ledCount - j - 1 == (millis() - config.displayStateLastUpdated)/(int)(500/(config.ledCount/2.0))){
            ledStrip->SetPixelColor(j, RgbColor(red, green, blue));
          }else{
            ledStrip->SetPixelColor(j, RgbColor(0x00, 0x00, 0x00));
          }
        }
      }else if(config.displayState == DS_VOLTAGE){
        if(config.batteryVoltage >= 4.00){
          green = 255;
        }else if(config.batteryVoltage <= 3.50){
          green = 0;
        }else{
          green = (((config.batteryVoltage - 3.50) * 2) * 255);
        } 
        red = 0xff - green;
        blue = 0x00;
        for (int j=0; j<config.ledCount; j++){
          ledStrip->SetPixelColor(j, RgbColor(red, green, blue));
        }
      }else if(config.displayState == DS_VOLTAGE2){
        if(config.batteryVoltage > 3.90){
          red = 0x00;
          green = 0xff;
          blue = 0x00;
        }else if(config.batteryVoltage > 3.50){
          red = 0xAA;
          green = 0xAA;
          blue = 0x00;
        }else{
          red = 0xFF;
          green = 0x00;
          blue = 0x00;
        }
        
        for (int j=0; j<(int)config.batteryVoltage; j++){
          ledStrip->SetPixelColor(j, RgbColor(0, 0, 255));
        }
        for (int j=0; j<(int)((config.batteryVoltage - (int)config.batteryVoltage) * 10); j++){
          if(j > 5){
            ledStrip->SetPixelColor(j+11, RgbColor(red, green, blue)); 
          }else if(j > 2){
            ledStrip->SetPixelColor(j+10, RgbColor(red, green, blue)); 
          }else{
            ledStrip->SetPixelColor(j+9, RgbColor(red, green, blue)); 
          }
        }
      }else if(config.displayState == DS_SHUTDOWN){
        if(config.batteryVoltage >= 4.00){
          green = 255;
        }else if(config.batteryVoltage <= 3.50){
          green = 0;
        }else{
          green = (((config.batteryVoltage - 3.50) * 2) * 255);
        } 
        red = (0xff - green);
        blue = 0x00;
        // 2000ms Blink & Pixel Crush
        if (millis() - config.displayStateLastUpdated > 200) {
          for(int j = 0; j < config.ledCount; j++){
            int threshold = (int)(((millis() - config.displayStateLastUpdated) / 2000.0) * ((config.ledCount/2) + config.ledCount % 2)) -1;
            if(j > threshold && j < config.ledCount - threshold - 1){
              ledStrip->SetPixelColor(j, RgbColor(red, green, blue));
            }
          }
        }
      }else if(config.displayState == DS_BANK){
        int chunkSize = max(2, (config.ledCount - 2)/4);
        int pressTime = (millis() - config.displayStateLastUpdated) % 4500;
        if (pressTime < 2000){
          for (int j=1; j-1 <= pressTime/500; j+=1){
            for(int k=(j-1) * chunkSize; k < j*chunkSize -1; k++){
              ledStrip->SetPixelColor(k, RgbColor(0xFF, 0x00, 0xFF));
            }
          }
        }else{
          for (int j=0; j < 4; j+=1){
            for(int k=j * chunkSize; k < ((j + 1) * chunkSize) -1; k++){
                ledStrip->SetPixelColor(k, RgbColor(0xFF, 0x00, 0xFF));
                if (pressTime < 2500){
                  if(chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2)){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }else if (pressTime < 3000){
                  if(j == 0 && (chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2))){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }else if (pressTime < 3500){
                  if(j == 1 && (chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2))){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }else if (pressTime < 4000){
                  if(j == 2 && (chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2))){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }else if (pressTime < 4500){
                  if(j == 3 && (chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2))){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }
            }
          }
        }
      }
else if(config.displayState == DS_BRIGHTNESS){
        // Override brightness without saving it. Button will save it upon release.
        if(millis() - config.displayStateLastUpdated < 500){
          config.ledBrightness = config.ledBrightnessOptions[0];
        }else if(millis() - config.displayStateLastUpdated < 1000){
          config.ledBrightness = config.ledBrightnessOptions[1];
        }else if(millis() - config.displayStateLastUpdated < 1500){
          config.ledBrightness = config.ledBrightnessOptions[2];
        }else if(millis() - config.displayStateLastUpdated < 2000){
          config.ledBrightness = config.ledBrightnessOptions[3];
        }else if(millis() - config.displayStateLastUpdated < 2500){
          config.ledBrightness = config.ledBrightnessOptions[4];
        }else{
          config.ledBrightness = config.ledBrightnessOptions[5];
        }
        ledStrip->SetBrightness(config.ledBrightness);
        red = 0xFF;
        green = 0xFF;
        blue = 0xFF;
        for (int j=0; j< config.ledCount; j++){
          if (j % 4 == 1 || j % 4 == 2 || config.ledCount < 8){
            ledStrip->SetPixelColor(j, RgbColor(red, green, blue));
          }
        }
      }else if(config.displayState == DS_SPEED){
        red = 0xFF;
        if(config.ledCount < 6 && (millis() - config.displayStateLastUpdated) % 500 < 25 && (millis() - config.displayStateLastUpdated) < 3000){
          for (int j=0; j< config.ledCount; j++){
            ledStrip->SetPixelColor(j, RgbColor(red, 0, 0));
          }
        }else{
          for (int j=0; j < min(int((millis() - config.displayStateLastUpdated + 500)/500), 6) * int(config.ledCount/6); j++){
            ledStrip->SetPixelColor(j, RgbColor(red, 0, 0));
          }
        }
      }else if(config.displayState == DS_PALETTE_MENU){
        // Live Visual Palette Preview Menu while holding Click 5
        // 25 Palettes, 400ms each (10,000ms full loop)
        int palIndex = ((millis() - config.displayStateLastUpdated) / 400) % 25;
        for (int j=0; j<config.ledCount; j++){
          uint8_t sampleLum = (uint8_t)((j * 255) / max(1, config.ledCount - 1));
          uint8_t r = sampleLum, g = sampleLum, b = sampleLum;
          applyPaletteFX(r, g, b, palIndex, millis(), config.paletteSpeed);
          ledStrip->SetPixelColor(j, RgbColor(r, g, b));
        }
      }


      // Super low voltage, only display red
      if(config.batteryState == BAT_CRITICAL && (config.displayState == DS_PATTERN || config.displayState == DS_PATTERN_ALL)){
        ledStrip->ClearTo(RgbColor(0,0,0));
        ledStrip->SetPixelColor(0, RgbColor(255, 0x00, 0x00));
        ledStrip->SetPixelColor(config.ledCount - 1, RgbColor(255, 0x00, 0x00));
      }

      // Output
      ledStrip->Show();
    }
};

#endif
