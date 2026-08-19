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

// Fast Zero-RAM Spectrum Converter (Preserves Black & Negative Space)
static inline void hueToRgb(uint8_t hue, uint8_t lum, uint8_t& r, uint8_t& g, uint8_t& b) {
  if (lum == 0) { r = 0; g = 0; b = 0; return; }
  uint8_t region = hue / 43;
  uint8_t rem = (uint8_t)((hue - (region * 43)) * 6);
  uint8_t q = (uint8_t)((lum * (255 - rem)) >> 8);
  uint8_t t = (uint8_t)((lum * rem) >> 8);

  switch (region % 6) {
    case 0: r = lum; g = t;   b = 0;   break;
    case 1: r = q;   g = lum; b = 0;   break;
    case 2: r = 0;   g = lum; b = t;   break;
    case 3: r = 0;   g = q;   b = lum; break;
    case 4: r = t;   g = 0;   b = lum; break;
    default: r = lum; g = 0;  b = q;   break;
  }
}

// 20 Base Color Palettes (Normalized 0-255 index -> RGB)
static inline void getPaletteBaseColor(uint8_t palId, uint8_t index256, uint8_t lum, uint8_t& red, uint8_t& green, uint8_t& blue) {
  if (lum == 0) { red = 0; green = 0; blue = 0; return; }

  switch (palId) {
    case 1: { // 1. 🌈 Rainbow Spectrum (360° Hue Wheel)
      hueToRgb(index256, lum, red, green, blue);
      break;
    }
    case 2: { // 2. 🌡️ FLIR Thermal Heat (Indigo -> Crimson -> Orange -> Solar Yellow -> White)
      if (index256 < 64) {
        red = (uint8_t)(lum * index256 / 64); green = 0; blue = (uint8_t)(lum * (64 - index256) / 64);
      } else if (index256 < 140) {
        uint8_t t = (index256 - 64) * 255 / 76;
        red = lum; green = (uint8_t)((lum * t * 160) >> 16); blue = 0;
      } else if (index256 < 210) {
        uint8_t t = (index256 - 140) * 255 / 70;
        red = lum; green = (uint8_t)(160 + (t * 95 >> 8)); blue = (uint8_t)((lum * t * 80) >> 16);
      } else {
        red = lum; green = lum; blue = lum;
      }
      break;
    }
    case 3: { // 3. 🪩 Prismatic Liquid Opal / Oil Slick
      uint8_t hue = (uint8_t)((index256 * 3) & 0xFF);
      hueToRgb(hue, lum, red, green, blue);
      red = (uint8_t)((red + lum) >> 1);
      green = (uint8_t)((green + lum) >> 1);
      blue = (uint8_t)((blue + lum) >> 1);
      break;
    }
    case 4: { // 4. 🌆 Cyberpunk Outrun 80s (Laser Cyan & Hot Pink)
      if (index256 < 128) {
        red = 0; green = lum; blue = lum; // Cyan
      } else {
        red = lum; green = 0; blue = (uint8_t)((lum * 180) >> 8); // Hot Pink
      }
      break;
    }
    case 5: { // 5. 🌌 Synthwave Sunset Horizon (Twilight Purple -> Sunset Magenta -> Goldenrod)
      if (index256 < 85) {
        red = (uint8_t)((lum * 160) >> 8); green = 0; blue = lum; // Twilight Purple
      } else if (index256 < 170) {
        red = lum; green = (uint8_t)((lum * 20) >> 8); blue = (uint8_t)((lum * 120) >> 8); // Magenta
      } else {
        red = lum; green = (uint8_t)((lum * 160) >> 8); blue = 0; // Sunburst Gold
      }
      break;
    }
    case 6: { // 6. 🌋 Molten Magma Lava (Volcanic Crimson -> Flame Orange -> White-Hot)
      if (index256 < 100) {
        red = lum; green = 0; blue = 0;
      } else if (index256 < 200) {
        red = lum; green = (uint8_t)((lum * (index256 - 100) * 180) / 100); blue = 0;
      } else {
        red = lum; green = lum; blue = (uint8_t)((lum * (index256 - 200) * 255) / 55);
      }
      break;
    }
    case 7: { // 7. 🟩 Matrix Digital Phosphor (Deep Green -> Neon Lime -> White Sparks)
      if (index256 > 230) {
        red = lum; green = lum; blue = lum;
      } else {
        red = (uint8_t)((lum * 15) >> 8);
        green = (uint8_t)((lum * (140 + (index256 % 115))) >> 8);
        blue = (uint8_t)((lum * 25) >> 8);
      }
      break;
    }
    case 8: { // 8. ❄️ Glacial Arctic Ice (Aqua Blue -> Ice Blue -> Diamond White)
      if (index256 > 220) {
        red = lum; green = lum; blue = lum;
      } else {
        red = (uint8_t)((lum * (index256 >> 2)) >> 8);
        green = (uint8_t)((lum * (140 + (index256 >> 1))) >> 8);
        blue = lum;
      }
      break;
    }
    case 9: { // 9. ☣️ Radioactive Toxic Acid (Neon Lime & Chartreuse)
      red = (uint8_t)((lum * (120 + (index256 % 120))) >> 8);
      green = lum;
      blue = 0;
      break;
    }
    case 10: { // 10. 🍬 Cotton Candy Mirage (Pastel Rose Pink & Sky Blue)
      if (index256 < 128) {
        red = lum; green = (uint8_t)((lum * 100) >> 8); blue = (uint8_t)((lum * 180) >> 8);
      } else {
        red = (uint8_t)((lum * 100) >> 8); green = (uint8_t)((lum * 180) >> 8); blue = lum;
      }
      break;
    }
    case 11: { // 11. 👑 Royal Champagne Gold (Warm Gold & Platinum Glints)
      if (index256 > 220) {
        red = lum; green = lum; blue = (uint8_t)((lum * 200) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * 180) >> 8); blue = (uint8_t)((lum * 40) >> 8);
      }
      break;
    }
    case 12: { // 12. 🌸 Cherry Blossom Sakura (Petal White -> Rose Pink -> Magenta)
      if (index256 < 100) {
        red = lum; green = (uint8_t)((lum * 200) >> 8); blue = (uint8_t)((lum * 220) >> 8);
      } else if (index256 < 200) {
        red = lum; green = (uint8_t)((lum * 100) >> 8); blue = (uint8_t)((lum * 160) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * 30) >> 8); blue = (uint8_t)((lum * 120) >> 8);
      }
      break;
    }
    case 13: { // 13. 🩸 Blood Moon Eclipse (Deep Ruby -> Crimson Fire -> Amber Corona)
      if (index256 < 180) {
        red = lum; green = (uint8_t)((lum * index256 * 40) / 180); blue = 0;
      } else {
        red = lum; green = (uint8_t)((lum * (40 + (index256 - 180) * 2)) >> 8); blue = 0;
      }
      break;
    }
    case 14: { // 14. 💚 Emerald Cyber Crystal (Forest Green -> Neon Mint)
      red = (uint8_t)((lum * (index256 > 200 ? (index256 - 200) * 2 : 0)) >> 8);
      green = lum;
      blue = (uint8_t)((lum * (index256 % 160)) >> 8);
      break;
    }
    case 15: { // 15. ☀️ Solar Flare Sunburst (Radioactive Yellow & Blazing Orange)
      if (index256 > 230) {
        red = lum; green = lum; blue = lum;
      } else {
        red = lum; green = (uint8_t)((lum * (160 + (index256 % 80))) >> 8); blue = 0;
      }
      break;
    }
    case 16: { // 16. 🩵 High-Tech Cyber Turquoise (Electric Cyan & Turquoise Spark)
      red = (uint8_t)((lum * (index256 > 220 ? 180 : 0)) >> 8);
      green = lum;
      blue = lum;
      break;
    }
    case 17: { // 17. 💜 Mystic Amethyst Glow (Deep Violet & Neon Orchid)
      red = (uint8_t)((lum * (160 + (index256 % 95))) >> 8);
      green = (uint8_t)((lum * (index256 > 220 ? 100 : 0)) >> 8);
      blue = lum;
      break;
    }
    case 18: { // 18. 🍯 Liquid Honey Amber (Golden Amber Shimmer)
      red = lum;
      green = (uint8_t)((lum * (130 + (index256 % 80))) >> 8);
      blue = 0;
      break;
    }
    case 19: { // 19. 💥 Electric Violet Lightning (Deep Indigo & Electric White Arcs)
      if (index256 > 220) {
        red = lum; green = lum; blue = lum;
      } else {
        red = (uint8_t)((lum * 200) >> 8); green = 0; blue = lum;
      }
      break;
    }
    case 20: { // 20. 🖤 Dark Matter Supernova (Void Black & Corona Flares)
      if (index256 > 220) {
        red = lum; green = lum; blue = lum;
      } else {
        red = lum; green = (uint8_t)((lum * (index256 % 180)) >> 8); blue = 0;
      }
      break;
    }
    default: {
      hueToRgb(index256, lum, red, green, blue);
      break;
    }
  }
}

// Modular Palette + Motion Flow FX Engine
static inline void applyModularPaletteFX(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t palId, uint8_t motionId, unsigned long timeMs, uint8_t speed, uint8_t ledIndex = 0, uint16_t frameIndex = 0, uint16_t frameCount = 1, uint8_t ledCount = 55) {
  if (palId == 0) return; // 0: Normal True-Color RGB

  // Negative Space Protection: Never alter black pixels
  if (red == 0 && green == 0 && blue == 0) return;
  uint8_t lum = (uint8_t)((red * 77 + green * 150 + blue * 29) >> 8);
  if (lum == 0) { red = 0; green = 0; blue = 0; return; }

  uint16_t safeFrameCount = (frameCount > 0) ? frameCount : 1;
  uint8_t safeLedCount = (ledCount > 0) ? ledCount : 55;
  uint8_t index256 = 0;

  switch (motionId) {
    case 0: { // 0. ⚪ No Effect (Pure Static Palette Tone Remap / No Motion)
      index256 = lum;
      break;
    }
    case 1: { // 1. ⬆️ Flow UP (Geyser Outward to Tip)
      index256 = (uint8_t)(((timeMs * speed / 12) - (ledIndex * 256 / safeLedCount)) & 0xFF);
      break;
    }
    case 2: { // 2. ⬇️ Flow DOWN (Cascade Inward to Handle)
      index256 = (uint8_t)(((timeMs * speed / 12) + (ledIndex * 256 / safeLedCount)) & 0xFF);
      break;
    }
    case 3: { // 3. 🌧️ Matrix Pixel Rain (Falling Sparks)
      index256 = (uint8_t)(((timeMs * speed / 10) + (ledIndex * 14)) & 0xFF);
      break;
    }
    case 4: { // 4. 🌊 Dual Harmonic Tidal Surge (Up & Down Sine Waves)
      int waveUp = (int)((timeMs * speed / 14) - (ledIndex * 8));
      int waveDown = (int)((timeMs * speed / 16) + (ledIndex * 8));
      index256 = (uint8_t)((waveUp ^ waveDown) & 0xFF);
      break;
    }
    case 5: { // 5. ⚡ Plasma Lightning Burst (Radial Pulse)
      index256 = (uint8_t)(((timeMs * speed / 8) - (ledIndex * 16)) & 0xFF);
      break;
    }
    case 6: { // 6. 💫 Stardust Meteor Cascade
      index256 = (uint8_t)(((timeMs * speed / 10) + (ledIndex * 12)) & 0xFF);
      break;
    }
    case 7: { // 7. 💓 Chromatic Breathing Pulse
      index256 = (uint8_t)(((timeMs * speed / 18) + (ledIndex * 4)) & 0xFF);
      break;
    }
    case 8: { // 8. 🔄 Rotational POV Spin Sweep
      index256 = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      break;
    }
    case 9: { // 9. 🌀 3D Spiral Helix Vortex (Radial + Angular Flow)
      index256 = (uint8_t)(((timeMs * speed / 16) + (ledIndex * 6) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      break;
    }
    case 10: { // 10. ⚡ Hyper-Strobe Shimmer Blade
      bool strobe = ((frameIndex % 2) == 0) || ((millis() / 40) % 2 == 0);
      index256 = (uint8_t)((timeMs * speed / 18) & 0xFF);
      if (!strobe) lum = (uint8_t)(lum >> 2);
      break;
    }
    default: {
      index256 = (uint8_t)((ledIndex * 255) / max(1, safeLedCount - 1));
      break;
    }
  }

  getPaletteBaseColor(palId, index256, lum, red, green, blue);
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

    uint16_t getBlendDuration(uint8_t mode) {
      switch (mode) {
        case 1: return 220; // Fast Smooth Fade
        case 2: return 180; // Instant Energy Flash Burst
        case 3: return 260; // Crisp Curtain Wipe
        case 4: return 280; // Glowing Pulse Bloom
        case 5: return 240; // Iris Pop Center-Out
        case 6: return 240; // Dual-End Squeeze Inward
        case 7: return 220; // Cyber Glitch Strobe
        case 8: return 260; // Quantum Dissolve
        case 9: return 250; // Fast Comet Sweep
        default: return 200;
      }
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

        // Zero-RAM Transition Engine (strictly OFF when blendMode == 0)
        uint16_t currentDuration = getBlendDuration(config.blendMode);
        bool isTransitioning = (config.blendMode > 0 && config.blendStartTime > 0 && (millis() - config.blendStartTime < currentDuration));
        float tProgress = isTransitioning ? ((float)(millis() - config.blendStartTime) / (float)currentDuration) : 1.0f;


        for (int j=0; j<config.ledCount; j++){
          red = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 0];
          green = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 1];
          blue = config.pattern[frameIndex*config.frameHeight*3 + j%config.frameHeight*3 + 2];

          // Apply Snappy Zero-RAM Transition Modes
          if (isTransitioning) {
            if (config.blendMode == 1) {
              // 1. Fast Fade-In (Smooth linear ramp)
              red = (uint8_t)(red * tProgress);
              green = (uint8_t)(green * tProgress);
              blue = (uint8_t)(blue * tProgress);
            } else if (config.blendMode == 2) {
              // 2. Energy Flash Burst
              if (tProgress < 0.30f) {
                uint8_t burst = (uint8_t)((1.0f - (tProgress / 0.30f)) * 255);
                red = min(255, (int)(red + burst));
                green = min(255, (int)(green + burst));
                blue = min(255, (int)(blue + burst));
              }
            } else if (config.blendMode == 3) {
              // 3. Fast Curtain Wipe In (Top to bottom)
              int wipeLimit = (int)(tProgress * config.ledCount);
              if (j > wipeLimit) {
                red = 0; green = 0; blue = 0;
              }
            } else if (config.blendMode == 4) {
              // 4. Glow Pulse In
              float pulse = 0.3f + 0.7f * tProgress;
              red = (uint8_t)(red * pulse);
              green = (uint8_t)(green * pulse);
              blue = (uint8_t)(blue * pulse);
            } else if (config.blendMode == 5) {
              // 5. Iris Burst (Center-Out expand)
              int center = config.ledCount / 2;
              int radius = (int)(tProgress * (config.ledCount / 2 + 1));
              if (abs(j - center) > radius) {
                red = 0; green = 0; blue = 0;
              }
            } else if (config.blendMode == 6) {
              // 6. Dual-End Squeeze In (Top & Bottom inward to center)
              int edgeReach = (int)(tProgress * (config.ledCount / 2 + 1));
              int distFromEnd = min(j, config.ledCount - 1 - j);
              if (distFromEnd > edgeReach) {
                red = 0; green = 0; blue = 0;
              }
            } else if (config.blendMode == 7) {
              // 7. Strobe Sparkle / Glitch Shimmer
              bool strobeOn = ((int)(tProgress * 20) % 2 == 0);
              if (!strobeOn && tProgress < 0.70f) {
                red = (uint8_t)(red * 0.15f);
                green = (uint8_t)(green * 0.15f);
                blue = (uint8_t)(blue * 0.15f);
              } else {
                red = (uint8_t)(red * tProgress);
                green = (uint8_t)(green * tProgress);
                blue = (uint8_t)(blue * tProgress);
              }
            } else if (config.blendMode == 8) {
              // 8. Interleaved Quantum Dissolve (Odd pixels then Even pixels)
              if (j % 2 == 0) {
                float oddProg = min(1.0f, tProgress * 1.5f);
                red = (uint8_t)(red * oddProg);
                green = (uint8_t)(green * oddProg);
                blue = (uint8_t)(blue * oddProg);
              } else {
                float evenProg = max(0.0f, (tProgress - 0.33f) * 1.5f);
                red = (uint8_t)(red * evenProg);
                green = (uint8_t)(green * evenProg);
                blue = (uint8_t)(blue * evenProg);
              }
            } else if (config.blendMode == 9) {
              // 9. Fast Cascading Comet Sweep
              int cometPos = (int)(tProgress * (config.ledCount + 6));
              if (j > cometPos) {
                red = 0; green = 0; blue = 0;
              } else if (abs(j - cometPos) <= 2) {
                red = 255; green = 255; blue = 255; // White comet tip
              }
            }
          }



          // Apply Real-Time Color Palette & Motion Flow FX Engine (Zero-RAM Overhead)
          if (config.paletteFxMode > 0) {
            applyModularPaletteFX(red, green, blue, config.paletteFxMode, config.motionFxMode, millis(), config.paletteSpeed, j, frameIndex, config.frameCount, config.ledCount);
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
          // Multi-color spectrum indicator for bank selector
          uint8_t wave = (uint8_t)(((millis() - config.displayStateLastUpdated) * 255) / 500);
          hueToRgb(wave, 255, red, green, blue);
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
        // 5 Color-Coded Bank Zones across the strip:
        // Bank 1: 🟢 Neon Green (0x00, 0xFF, 0x55)
        // Bank 2: 🟡 Neon Yellow (0xFF, 0xD7, 0x00)
        // Bank 3: 🟣 Neon Purple (0xBC, 0x13, 0xFE)
        // Bank 4: 🌸 Neon Pink   (0xFF, 0x2E, 0x63)
        // Bank 5: 🩵 Neon Cyan   (0x00, 0xD2, 0xFF)
        const RgbColor bankColors[6] = {
          RgbColor(0x00, 0xFF, 0x55), // Bank 1: Neon Green
          RgbColor(0xFF, 0xD7, 0x00), // Bank 2: Neon Yellow
          RgbColor(0xBC, 0x13, 0xFE), // Bank 3: Neon Purple
          RgbColor(0xFF, 0x2E, 0x63), // Bank 4: Neon Pink
          RgbColor(0x00, 0xD2, 0xFF), // Bank 5: Neon Cyan
          RgbColor(0xFF, 0xFF, 0xFF)  // Bank 6: Diamond Laser White / Sacred Math
        };

        int totalLeds = config.ledCount > 0 ? config.ledCount : 55;
        int zoneSize = max(1, totalLeds / 6);
        int pressTime = (millis() - config.displayStateLastUpdated) % 6500;
        int activeSelection = pressTime / 500; // 0 to 12

        if (activeSelection < 6) {
          // Direct Bank Selection (0: Bank 1, 1: Bank 2, 2: Bank 3, 3: Bank 4, 4: Bank 5, 5: Bank 6)
          for (int b = 0; b < 6; b++) {
            int startIdx = b * zoneSize;
            int endIdx = (b == 5) ? totalLeds : (b + 1) * zoneSize;
            bool isSelected = (b == activeSelection);

            for (int k = startIdx; k < endIdx; k++) {
              if (isSelected) {
                // Highlight active selected bank with white pulsing shimmer
                bool pulse = ((millis() / 80) % 2 == 0);
                if (pulse) {
                  ledStrip->SetPixelColor(k, RgbColor(255, 255, 255));
                } else {
                  ledStrip->SetPixelColor(k, bankColors[b]);
                }
              } else {
                // Dim baseline illumination for other banks
                RgbColor dimCol = RgbColor(bankColors[b].R / 5, bankColors[b].G / 5, bankColors[b].B / 5);
                ledStrip->SetPixelColor(k, dimCol);
              }
            }
          }
        } else if (activeSelection == 6) {
          // 6: All-Bank Shuffle / Demo Tour (Rainbow Wave across all 6 zones)
          uint8_t waveOffset = (uint8_t)((millis() / 4) & 0xFF);
          for (int k = 0; k < totalLeds; k++) {
            uint8_t r, g, b;
            hueToRgb((uint8_t)(waveOffset + (k * 256 / totalLeds)), 255, r, g, b);
            ledStrip->SetPixelColor(k, RgbColor(r, g, b));
          }
        } else {
          // 7 to 12: Single-Bank Auto-Loop (7: Bank 1, 8: Bank 2, 9: Bank 3, 10: Bank 4, 11: Bank 5, 12: Bank 6)
          int loopBank = activeSelection - 7; // 0 to 5
          for (int b = 0; b < 6; b++) {
            int startIdx = b * zoneSize;
            int endIdx = (b == 5) ? totalLeds : (b + 1) * zoneSize;
            bool isLoopTarget = (b == loopBank);

            for (int k = startIdx; k < endIdx; k++) {
              if (isLoopTarget) {
                // Breathing strobe in bank color to indicate Loop Mode
                uint8_t breathe = (uint8_t)(128 + 127 * sin((millis() % 500) * 3.14159 / 250.0));
                RgbColor loopCol = RgbColor((bankColors[b].R * breathe) >> 8, (bankColors[b].G * breathe) >> 8, (bankColors[b].B * breathe) >> 8);
                ledStrip->SetPixelColor(k, loopCol);
              } else {
                ledStrip->SetPixelColor(k, RgbColor(0, 0, 0));
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
        int palIndex = ((millis() - config.displayStateLastUpdated) / 400) % 20 + 1;
        for (int j=0; j<config.ledCount; j++){
          uint8_t sampleLum = (uint8_t)((j * 255) / max(1, config.ledCount - 1));
          uint8_t r = sampleLum, g = sampleLum, b = sampleLum;
          applyModularPaletteFX(r, g, b, palIndex, 0, millis(), config.paletteSpeed, j, 0, 1, config.ledCount);
          ledStrip->SetPixelColor(j, RgbColor(r, g, b));
        }

      }else if(config.displayState == DS_PALETTE_SELECT){
        // Dedicated Pure Solid / Flowing Palette Preview across the full LED blade
        int totalLeds = config.ledCount > 0 ? config.ledCount : 55;
        for (int j=0; j<totalLeds; j++){
          uint8_t r = 255, g = 255, b = 255;
          applyModularPaletteFX(r, g, b, config.paletteFxMode, config.motionFxMode, millis(), config.paletteSpeed, j, 0, 1, totalLeds);
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
