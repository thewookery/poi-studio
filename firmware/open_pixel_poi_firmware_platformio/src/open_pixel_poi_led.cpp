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

// 25 Zero-RAM Real-Time Harmonic & Flowing Color Palettes (Never touches Black / Blank Negative Space)
static inline void applyPaletteFX(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t mode, unsigned long timeMs, uint8_t speed, uint8_t ledIndex = 0, uint16_t frameIndex = 0, uint16_t frameCount = 1, uint8_t ledCount = 55) {
  if (mode == 0) return; // 0: Normal RGB (Passthrough)

  // IRONCLAD NEGATIVE SPACE PROTECTION: Never alter black pixels or blank space!
  if (red == 0 && green == 0 && blue == 0) return;
  uint8_t lum = (uint8_t)((red * 77 + green * 150 + blue * 29) >> 8);
  if (lum == 0) { red = 0; green = 0; blue = 0; return; }

  uint16_t safeFrameCount = (frameCount > 0) ? frameCount : 1;
  uint8_t safeLedCount = (ledCount > 0) ? ledCount : 55;

  switch (mode) {
    case 1: { // 1. 🌈 RAINBOW SPIN VORTEX (Continuous 360° POV Rainbow)
      uint8_t hue = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      hueToRgb(hue, lum, red, green, blue);
      break;
    }
    case 2: { // 2. 🌊 RAINBOW BLADE RIPPLE (Outward radial wave from core to tip)
      uint8_t hue = (uint8_t)(((timeMs * speed / 16) + (ledIndex * 256 / safeLedCount)) & 0xFF);
      hueToRgb(hue, lum, red, green, blue);
      break;
    }
    case 3: { // 3. 🌀 RAINBOW SPIRAL VORTEX (Harmonic dual radial + angular flow)
      uint8_t hue = (uint8_t)(((timeMs * speed / 16) + (ledIndex * 4) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      hueToRgb(hue, lum, red, green, blue);
      break;
    }
    case 4: { // 4. 💓 CHROMATIC BREATHING PULSE (Sinusoidal saturation & hue breathe)
      uint8_t hueShift = (uint8_t)((timeMs * speed / 20) & 0xFF);
      uint8_t breatheLum = (uint8_t)((lum * (180 + ((timeMs * speed / 8) % 75))) / 255);
      hueToRgb(hueShift, breatheLum, red, green, blue);
      break;
    }
    case 5: { // 5. 👁️ HOLOGRAPHIC CHROMATIC ABERRATION (3D RGB Spatial Prism Split)
      uint8_t rPhase = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount) + ledIndex * 2) & 0xFF);
      uint8_t gPhase = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      uint8_t bPhase = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount) - ledIndex * 2) & 0xFF);
      red   = (uint8_t)((lum * (120 + ((rPhase % 135)))) >> 8);
      green = (uint8_t)((lum * (120 + ((gPhase % 135)))) >> 8);
      blue  = (uint8_t)((lum * (120 + ((bPhase % 135)))) >> 8);
      break;
    }
    case 6: { // 6. 🌀 KALEIDOSCOPIC MOIRÉ INTERFERENCE (Dual Counter-Rotating Harmonic Bands)
      int wave1 = (int)((timeMs * speed / 14) + (frameIndex * 768 / safeFrameCount) + (ledIndex * 6));
      int wave2 = (int)((timeMs * speed / 18) - (frameIndex * 1280 / safeFrameCount) - (ledIndex * 8));
      uint8_t moireHue = (uint8_t)((wave1 ^ wave2) & 0xFF);
      hueToRgb(moireHue, lum, red, green, blue);
      break;
    }
    case 7: { // 7. 🌡️ FLIR THERMAL HEAT VISION (Black -> Indigo -> Crimson -> Orange -> Solar -> White)
      if (lum < 45) {
        red = (uint8_t)(lum * 2); green = 0; blue = (uint8_t)(lum * 5);
      } else if (lum < 110) {
        uint8_t t = (lum - 45) * 255 / 65;
        red = (uint8_t)(90 + (t * 165 >> 8)); green = 0; blue = (uint8_t)(225 - (t * 200 >> 8));
      } else if (lum < 190) {
        uint8_t t = (lum - 110) * 255 / 80;
        red = 255; green = (uint8_t)(t * 220 >> 8); blue = 0;
      } else {
        uint8_t t = (lum - 190) * 255 / 65;
        red = 255; green = 255; blue = (uint8_t)(t * 255 >> 8);
      }
      break;
    }
    case 8: { // 8. 🪩 PRISMATIC OPAL & IRIDESCENT OIL SLICK (Refractive Phase Sheen)
      uint8_t opalAngle = (uint8_t)(((timeMs * speed / 20) + (ledIndex * 12) + (frameIndex * 512 / safeFrameCount)) & 0xFF);
      uint8_t opalHue = (uint8_t)((opalAngle * 3) & 0xFF);
      uint8_t opalLum = (uint8_t)((lum * (200 + (opalAngle % 55))) >> 8);
      hueToRgb(opalHue, opalLum, red, green, blue);
      red = (uint8_t)((red + lum) >> 1);
      green = (uint8_t)((green + lum) >> 1);
      blue = (uint8_t)((blue + lum) >> 1);
      break;
    }
    case 9: { // 9. ⚡ HYPER-DRIVE WARP PLASMA (Deep Core -> Blinding White-Blue Lightning Edge)
      float radialPower = (float)ledIndex / (float)safeLedCount;
      uint8_t tipSparks = (uint8_t)(((timeMs * speed / 10) + (ledIndex * 14) + (frameIndex * 120)) & 0xFF);
      if (radialPower < 0.4f) {
        red = (uint8_t)((lum * 20) >> 8); green = (uint8_t)((lum * 60) >> 8); blue = lum;
      } else {
        uint8_t boost = (uint8_t)((radialPower - 0.4f) * 1.66f * 255);
        red = (uint8_t)((lum * (40 + (boost * 215 >> 8))) >> 8);
        green = (uint8_t)((lum * (100 + (boost * 155 >> 8))) >> 8);
        blue = lum;
        if (tipSparks > 230 && radialPower > 0.7f) {
          red = 255; green = 255; blue = 255;
        }
      }
      break;
    }
    case 10: { // 10. 🖤 DARK MATTER SUPERNOVA (High-Energy Corona Flares on Negative Void)
      uint8_t corona = (uint8_t)(((timeMs * speed / 14) + (frameIndex * 256 / safeFrameCount) + (ledIndex * 6)) & 0xFF);
      red = lum;
      green = (uint8_t)((lum * (corona % 180)) >> 8);
      blue = (uint8_t)((lum * (corona > 220 ? (corona - 220) * 6 : 0)) >> 8);
      break;
    }
    case 11: { // 11. 🔮 ABYSSAL BIOLUMINESCENT PLANKTON (Phosphorescent Spark Trails)
      uint8_t spark = (uint8_t)(((timeMs * speed / 12) + (ledIndex * 10) - (frameIndex * 192 / safeFrameCount)) & 0xFF);
      red = (uint8_t)((lum * (spark > 235 ? 200 : 0)) >> 8);
      green = (uint8_t)((lum * (160 + (spark % 95))) >> 8);
      blue = (uint8_t)((lum * (200 + (spark % 55))) >> 8);
      break;
    }
    case 12: { // 12. 🌆 SYNTHWAVE OUTRUN NEON GRID (Laser Cyan, Hot Pink, Sunset Amber)
      uint8_t grid = (uint8_t)(((timeMs * speed / 18) + (ledIndex * 8) + (frameIndex * 128 / safeFrameCount)) & 0xFF);
      if (grid < 85) {
        red = 0; green = lum; blue = lum;
      } else if (grid < 170) {
        red = lum; green = (uint8_t)((lum * 20) >> 8); blue = (uint8_t)((lum * 180) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * 140) >> 8); blue = 0;
      }
      break;
    }
    case 13: { // 13. 🔥 LIQUID LAVA MOLTEN FLOW (Molten Magma Stream)
      uint8_t heat = (uint8_t)(((timeMs * speed / 16) + (ledIndex * 5) + (frameIndex * 128 / safeFrameCount)) & 0xFF);
      uint8_t hLum = (uint8_t)((lum * (200 + (heat % 55))) / 255);
      if (heat < 85) {
        red = hLum; green = (uint8_t)((hLum * heat * 3) >> 8); blue = 0;
      } else if (heat < 170) {
        red = hLum; green = (uint8_t)((hLum * (128 + ((heat - 85) * 127 / 85))) >> 8); blue = 0;
      } else {
        red = hLum; green = hLum; blue = (uint8_t)((hLum * (heat - 170) * 3) >> 8);
      }
      break;
    }
    case 14: { // 14. 🟩 MATRIX DIGITAL PHOSPHOR RAIN
      uint8_t rain = (uint8_t)(((timeMs * speed / 12) + (ledIndex * 8) + (frameIndex * 64 / safeFrameCount)) & 0xFF);
      if (rain > 220) { red = (uint8_t)((lum * 200) >> 8); green = lum; blue = (uint8_t)((lum * 200) >> 8); }
      else { red = (uint8_t)((lum * 15) >> 8); green = (uint8_t)((lum * (160 + (rain % 95))) >> 8); blue = (uint8_t)((lum * 25) >> 8); }
      break;
    }
    case 15: { // 15. ❄️ GLACIAL ARCTIC AURORA (Arctic Aqua -> Ice Blue -> Diamond White)
      uint8_t aurora = (uint8_t)(((timeMs * speed / 18) + (ledIndex * 4) + (frameIndex * 128 / safeFrameCount)) & 0xFF);
      red = (uint8_t)((lum * (80 + (aurora % 140))) >> 8);
      green = (uint8_t)((lum * (180 + (aurora % 75))) >> 8);
      blue = lum;
      break;
    }
    case 16: { // 16. 🌅 SUNSET TWILIGHT HORIZON (Twilight Purple -> Crimson -> Warm Gold)
      uint8_t dusk = (uint8_t)(((timeMs * speed / 20) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      if (dusk < 128) {
        red = (uint8_t)((lum * (150 + (dusk * 105 / 128))) >> 8);
        green = (uint8_t)((lum * (dusk * 120 / 128)) >> 8);
        blue = (uint8_t)((lum * (220 - (dusk * 180 / 128))) >> 8);
      } else {
        red = lum;
        green = (uint8_t)((lum * (120 + ((dusk - 128) * 110 / 127))) >> 8);
        blue = (uint8_t)((lum * 40) >> 8);
      }
      break;
    }
    case 17: { // 17. 🍧 VAPORWAVE PASTEL WAVE (Coral Pink & Mint Turquoise)
      uint8_t phase = (uint8_t)(((timeMs * speed / 18) + (ledIndex * 6)) & 0xFF);
      red = (uint8_t)((lum * (160 + ((phase % 95)))) >> 8);
      green = (uint8_t)((lum * (100 + (((255 - phase) % 120)))) >> 8);
      blue = (uint8_t)((lum * (200 + ((phase % 55)))) >> 8);
      break;
    }
    case 18: // 18. 🔳 MONOCHROME PHOSPHOR (High-Contrast Pure B&W)
      red = lum; green = lum; blue = lum;
      break;
    case 19: { // 19. 🍯 AMBER GOLD SHIMMER (Liquid Amber & Honey Wave)
      uint8_t wave = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 128 / safeFrameCount)) & 0xFF);
      red = lum;
      green = (uint8_t)((lum * (140 + (wave % 80))) >> 8);
      blue = 0;
      break;
    }
    case 20: { // 20. 🌊 DEEP OCEAN ABYSSAL WAVE
      uint8_t ocean = (uint8_t)(((timeMs * speed / 16) + (ledIndex * 6) + (frameIndex * 64 / safeFrameCount)) & 0xFF);
      red = 0;
      green = (uint8_t)((lum * (100 + (ocean % 130))) >> 8);
      blue = lum;
      break;
    }
    case 21: { // 21. 🌋 VOLCANIC OBSIDIAN MAGMA (Molten Gold & Volcanic Crimson)
      uint8_t lava = (uint8_t)(((timeMs * speed / 16) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      red = lum;
      green = (uint8_t)((lum * (lava % 190)) >> 8);
      blue = (uint8_t)((lum * (lava > 200 ? (lava - 200) : 0)) >> 8);
      break;
    }
    case 22: { // 22. ☣️ RADIOACTIVE TOXIC ACID (Neon Lime & Chartreuse)
      uint8_t tox = (uint8_t)(((timeMs * speed / 14) + (ledIndex * 8)) & 0xFF);
      red = (uint8_t)((lum * (120 + (tox % 120))) >> 8);
      green = lum;
      blue = 0;
      break;
    }
    case 23: { // 23. 🍬 COTTON CANDY SWIRL (Pastel Pink & Sky Blue)
      uint8_t cc = (uint8_t)(((timeMs * speed / 18) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      red = (uint8_t)((lum * (180 + (cc % 75))) >> 8);
      green = (uint8_t)((lum * 80) >> 8);
      blue = (uint8_t)((lum * (180 + ((255 - cc) % 75))) >> 8);
      break;
    }
    case 24: { // 24. 👑 ROYAL GOLD & PLATINUM (Champagne Gold & Platinum)
      uint8_t gold = (uint8_t)(((timeMs * speed / 18) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      red = lum;
      green = (uint8_t)((lum * (180 + (gold % 70))) >> 8);
      blue = (uint8_t)((lum * (gold % 160)) >> 8);
      break;
    }
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



          // Apply Real-Time Color Palette Filter (25 Zero-RAM Palettes with Black/Negative-Space Protection)
          if (config.paletteFxMode > 0) {
            applyPaletteFX(red, green, blue, config.paletteFxMode, millis(), config.paletteSpeed, j, frameIndex, config.frameCount, config.ledCount);
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
        int chunkSize = max(2, (config.ledCount - 2)/5);
        int pressTime = (millis() - config.displayStateLastUpdated) % 5500;
        if (pressTime < 2500){
          for (int j=1; j-1 <= pressTime/500; j+=1){
            for(int k=(j-1) * chunkSize; k < j*chunkSize -1; k++){
              ledStrip->SetPixelColor(k, RgbColor(0xFF, 0x00, 0xFF));
            }
          }
        }else{
          for (int j=0; j < 5; j+=1){
            for(int k=j * chunkSize; k < ((j + 1) * chunkSize) -1; k++){
                if (j == 4) {
                  ledStrip->SetPixelColor(k, RgbColor(0xFF, 0xD7, 0x00)); // Gold shimmer for Bank 5 Cosmic Tour
                } else {
                  ledStrip->SetPixelColor(k, RgbColor(0xFF, 0x00, 0xFF));
                }
                if (pressTime < 3000){
                  if(chunkSize <= 4 || (k - (j* chunkSize) > 0 && k - (j* chunkSize) < chunkSize - 2)){
                    ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                  }
                }else if (pressTime < 3500 && j == 0){
                  ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                }else if (pressTime < 4000 && j == 1){
                  ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                }else if (pressTime < 4500 && j == 2){
                  ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                }else if (pressTime < 5000 && j == 3){
                  ledStrip->SetPixelColor(k, RgbColor(0x00, 0x00, 0xFF));
                }else if (pressTime < 5500 && j == 4){
                  ledStrip->SetPixelColor(k, RgbColor(0x00, 0xF5, 0xD4)); // Turquoise glow for Bank 5
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
          applyPaletteFX(r, g, b, palIndex, millis(), config.paletteSpeed, j, 0, 1, config.ledCount);
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
