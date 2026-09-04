#ifndef _OPEN_PIXEL_POI_LED
#define _OPEN_PIXEL_POI_LED

#include <Arduino.h>

static inline uint8_t fastSin8(uint8_t theta) {
  uint8_t t = theta;
  if (t > 127) t = 255 - t;
  uint32_t val = (uint32_t)t * (255 - t);
  return (uint8_t)((val * 4) >> 8);
}


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

// 32 Curated Pro Color Palettes (Normalized 0-255 index -> RGB)
static inline void getPaletteBaseColor(uint8_t palId, uint8_t index256, uint8_t lum, uint8_t& red, uint8_t& green, uint8_t& blue) {
  if (lum == 0) { red = 0; green = 0; blue = 0; return; }

  switch (palId) {
    case 1: { // 1. 🌈 Rainbow Spectrum (360° Hue Wheel)
      hueToRgb(index256, lum, red, green, blue);
      break;
    }
    case 2: { // 2. 🌡️ FLIR Thermal Heat (Deep Indigo -> Crimson -> Flame Orange -> Solar Yellow -> White)
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
    case 4: { // 4. 🌆 Cyberpunk 2077 (Hot Magenta Pink -> Laser Purple -> Electric Cyan)
      if (index256 < 85) {
        red = lum; green = 0; blue = (uint8_t)((lum * 140) >> 8); // Hot Magenta
      } else if (index256 < 170) {
        uint8_t t = (index256 - 85) * 3;
        red = (uint8_t)((lum * (255 - t)) >> 8); green = (uint8_t)((lum * (t / 2)) >> 8); blue = lum; // Purple to Blue
      } else {
        uint8_t t = (index256 - 170) * 3;
        red = 0; green = (uint8_t)((lum * (128 + t / 2)) >> 8); blue = lum; // Electric Cyan
      }
      break;
    }
    case 5: { // 5. 🌌 Synthwave Sunset Horizon (Twilight Purple -> Sunset Magenta -> Sunburst Gold)
      if (index256 < 85) {
        red = (uint8_t)((lum * 160) >> 8); green = 0; blue = lum; // Twilight Purple
      } else if (index256 < 170) {
        red = lum; green = (uint8_t)((lum * 20) >> 8); blue = (uint8_t)((lum * 120) >> 8); // Magenta
      } else {
        red = lum; green = (uint8_t)((lum * 180) >> 8); blue = 0; // Sunburst Gold
      }
      break;
    }
    case 6: { // 6. 🌋 Molten Magma Lava (Volcanic Crimson -> Flame Orange -> Solar White-Hot)
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
    case 8: { // 8. ❄️ Glacial Arctic Ice (Deep Navy -> Electric Ice Blue -> Diamond White)
      if (index256 > 220) {
        red = lum; green = lum; blue = lum;
      } else if (index256 < 100) {
        red = 0; green = (uint8_t)((lum * index256 * 160) / 100); blue = lum;
      } else {
        red = (uint8_t)((lum * (index256 - 100) * 200) / 120); green = lum; blue = lum;
      }
      break;
    }
    case 9: { // 9. ☣️ Radioactive Toxic Acid (Neon Lime -> Chartreuse -> Electric Yellow)
      red = (uint8_t)((lum * (100 + ((index256 * 155) >> 8))) >> 8);
      green = lum;
      blue = (uint8_t)((lum * (index256 > 220 ? 120 : 0)) >> 8);
      break;
    }
    case 10: { // 10. 🍬 Cotton Candy Mirage (Pastel Rose Pink -> Bubblegum Lilac -> Sky Blue)
      if (index256 < 85) {
        red = lum; green = (uint8_t)((lum * 120) >> 8); blue = (uint8_t)((lum * 180) >> 8);
      } else if (index256 < 170) {
        red = (uint8_t)((lum * 200) >> 8); green = (uint8_t)((lum * 140) >> 8); blue = lum;
      } else {
        red = (uint8_t)((lum * 80) >> 8); green = (uint8_t)((lum * 200) >> 8); blue = lum;
      }
      break;
    }
    case 11: { // 11. 👑 Royal Champagne Gold (Deep Imperial Gold -> Warm Amber -> Platinum Glints)
      if (index256 > 220) {
        red = lum; green = lum; blue = (uint8_t)((lum * 220) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * (140 + ((index256 * 70) >> 8))) >> 8); blue = (uint8_t)((lum * 30) >> 8);
      }
      break;
    }
    case 12: { // 12. 🌸 Cherry Blossom Sakura (Petal White -> Rose Pink -> Vivid Magenta)
      if (index256 < 100) {
        red = lum; green = (uint8_t)((lum * 210) >> 8); blue = (uint8_t)((lum * 230) >> 8);
      } else if (index256 < 200) {
        red = lum; green = (uint8_t)((lum * 100) >> 8); blue = (uint8_t)((lum * 170) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * 20) >> 8); blue = (uint8_t)((lum * 140) >> 8);
      }
      break;
    }
    case 13: { // 13. 🩸 Blood Moon Eclipse (Obsidian Crimson -> Blood Ruby -> Solar Amber)
      if (index256 < 160) {
        red = (uint8_t)((lum * (80 + index256)) >> 8); green = 0; blue = 0;
      } else {
        red = lum; green = (uint8_t)((lum * (index256 - 160) * 160) / 95); blue = 0;
      }
      break;
    }
    case 14: { // 14. 💚 Emerald Cyber Crystal (Forest Emerald -> Jade Green -> Neon Mint)
      red = (uint8_t)((lum * (index256 > 180 ? (index256 - 180) * 2 : 0)) >> 8);
      green = lum;
      blue = (uint8_t)((lum * ((index256 * 200) >> 8)) >> 8);
      break;
    }
    case 15: { // 15. ☀️ Solar Flare Sunburst (Electric Lemon -> Blazing Orange -> Solar White)
      if (index256 > 230) {
        red = lum; green = lum; blue = lum;
      } else {
        red = lum; green = (uint8_t)((lum * (120 + ((index256 * 135) >> 8))) >> 8); blue = 0;
      }
      break;
    }
    case 16: { // 16. 🩵 High-Tech Cyber Turquoise (Electric Aqua -> Turquoise -> Pure Cyan)
      red = (uint8_t)((lum * (index256 > 200 ? (index256 - 200) * 2 : 0)) >> 8);
      green = (uint8_t)((lum * (160 + ((index256 * 95) >> 8))) >> 8);
      blue = lum;
      break;
    }
    case 17: { // 17. 💜 Mystic Amethyst & Orchid (Deep Violet -> Royal Purple -> Neon Orchid)
      red = (uint8_t)((lum * (120 + ((index256 * 135) >> 8))) >> 8);
      green = (uint8_t)((lum * (index256 > 210 ? (index256 - 210) * 2 : 0)) >> 8);
      blue = lum;
      break;
    }
    case 18: { // 18. 🫐 Blueberry Jam / Cosmic Indigo (Deep Indigo -> Cobalt Blue -> Electric Violet)
      if (index256 < 128) {
        red = (uint8_t)((lum * 40) >> 8); green = 0; blue = (uint8_t)((lum * (140 + index256)) >> 8);
      } else {
        uint32_t rVal = 40 + ((index256 - 128) * 3) / 2;
        red = (uint8_t)((lum * rVal) >> 8); green = (uint8_t)((lum * (index256 - 128)) >> 8); blue = lum;
      }
      break;
    }
    case 19: { // 19. 🍊 Tangerine Dream / Creamsicle (Sunset Orange -> Warm Peach -> Coral White)
      if (index256 > 220) {
        red = lum; green = (uint8_t)((lum * 220) >> 8); blue = (uint8_t)((lum * 180) >> 8);
      } else {
        red = lum; green = (uint8_t)((lum * (80 + ((index256 * 120) >> 8))) >> 8); blue = (uint8_t)((lum * (index256 >> 2)) >> 8);
      }
      break;
    }
    case 20: { // 20. 💎 Pure Diamond Strobe (Ultra-Crisp Diamond White -> Platinum Silver)
      uint32_t shimmer = 180 + ((index256 * 75) >> 8);
      red = (uint8_t)((lum * shimmer) >> 8);
      green = (uint8_t)((lum * shimmer) >> 8);
      blue = lum;
      break;
    }
    case 21: { // 21. 🦚 Peacock Feather (Royal Sapphire Blue -> Emerald Teal -> Lime Gold)
      if (index256 < 85) {
        red = 0; green = (uint8_t)((lum * index256 * 2) >> 8); blue = lum; // Sapphire to Teal
      } else if (index256 < 170) {
        uint32_t t = (index256 - 85) * 3;
        red = 0; green = lum; blue = (uint8_t)((lum * (255 - t)) >> 8); // Teal to Emerald
      } else {
        uint32_t t = (index256 - 170) * 3;
        red = (uint8_t)((lum * t) >> 8); green = lum; blue = 0; // Emerald to Lime Gold
      }
      break;
    }
    case 22: { // 22. ⚡ Hyper Neon Triad (Electric Lime -> Hot Magenta -> Laser Cyan)
      if (index256 < 85) {
        uint32_t t = index256 * 3;
        red = (uint8_t)((lum * t) >> 8); green = lum; blue = 0; // Lime to Yellow-Orange
      } else if (index256 < 170) {
        uint32_t t = (index256 - 85) * 3;
        red = lum; green = (uint8_t)((lum * (255 - t)) >> 8); blue = (uint8_t)((lum * t) >> 8); // Orange to Magenta
      } else {
        uint32_t t = (index256 - 170) * 3;
        red = (uint8_t)((lum * (255 - t)) >> 8); green = (uint8_t)((lum * t) >> 8); blue = lum; // Magenta to Cyan
      }
      break;
    }
    case 23: { // 23. 🏜️ Desert Mirage / Sedona (Terracotta Red -> Burnished Copper -> Golden Sand)
      if (index256 < 128) {
        red = lum; green = (uint8_t)((lum * (60 + index256 / 2)) >> 8); blue = (uint8_t)((lum * 20) >> 8);
      } else {
        uint32_t gVal = 124 + ((index256 - 128) * 4) / 5;
        uint32_t bVal = 20 + (index256 - 128) / 3;
        red = lum; green = (uint8_t)((lum * gVal) >> 8); blue = (uint8_t)((lum * bVal) >> 8);
      }
      break;
    }
    case 24: { // 24. 🖤 Blacklight UV Glow (Deep Ultra-Violet -> Fluorescent Purple -> Hot Violet)
      if (index256 < 128) {
        red = (uint8_t)((lum * (100 + index256 / 2)) >> 8); green = 0; blue = lum;
      } else {
        red = lum; green = (uint8_t)((lum * (index256 - 128) / 4) >> 8); blue = lum;
      }
      break;
    }
    case 25: { // 25. 🍉 Watermelon Wave (Ruby Red -> Crisp White -> Electric Lime Green)
      if (index256 < 100) {
        red = lum; green = 0; blue = (uint8_t)((lum * 30) >> 8); // Ruby Red
      } else if (index256 < 155) {
        red = lum; green = lum; blue = lum; // Crisp White Rind
      } else {
        red = (uint8_t)((lum * 30) >> 8); green = lum; blue = 0; // Lime Green
      }
      break;
    }
    case 26: { // 26. 🌌 Deep Nebula / Galaxy (Midnight Navy -> Magenta Dust -> Cyan Starfield)
      if (index256 < 85) {
        red = (uint8_t)((lum * 30) >> 8); green = (uint8_t)((lum * 10) >> 8); blue = (uint8_t)((lum * (120 + index256)) >> 8);
      } else if (index256 < 170) {
        uint32_t t = (index256 - 85) * 3;
        uint32_t rVal = 30 + (t * 4) / 5;
        uint32_t bVal = 205 - (t * 3) / 10;
        red = (uint8_t)((lum * rVal) >> 8); green = 0; blue = (uint8_t)((lum * bVal) >> 8);
      } else {
        uint32_t t = (index256 - 170) * 3;
        uint32_t rVal = 200 - (t * 7) / 10;
        uint32_t gVal = (t * 4) / 5;
        red = (uint8_t)((lum * rVal) >> 8); green = (uint8_t)((lum * gVal) >> 8); blue = lum;
      }
      break;
    }
    case 27: { // 27. 🍋 Electric Lemon-Lime (Laser Lemon -> Neon Lime -> Fresh Spring Green)
      if (index256 < 128) {
        red = lum; green = lum; blue = 0; // Laser Lemon
      } else {
        uint32_t t = (index256 - 128) * 2;
        red = (uint8_t)((lum * (255 - t)) >> 8); green = lum; blue = 0; // Neon Lime to Spring Green
      }
      break;
    }
    case 28: { // 28. 🏮 Tokyo Cyber Neon (Blood Orange -> Neon Fuchsia -> Laser Blue)
      if (index256 < 85) {
        red = lum; green = (uint8_t)((lum * 70) >> 8); blue = 0; // Blood Orange
      } else if (index256 < 170) {
        red = lum; green = 0; blue = (uint8_t)((lum * 160) >> 8); // Fuchsia
      } else {
        red = 0; green = (uint8_t)((lum * 140) >> 8); blue = lum; // Laser Blue
      }
      break;
    }
    case 29: { // 29. 🌊 Bioluminescent Abyss (Mariana Deep Blue -> Cyan Plankton -> Seafoam Green)
      if (index256 < 128) {
        uint32_t gVal = (index256 * 3) / 2;
        uint32_t bVal = 160 + (index256 * 7) / 10;
        red = 0; green = (uint8_t)((lum * gVal) >> 8); blue = (uint8_t)((lum * bVal) >> 8);
      } else {
        uint32_t t = (index256 - 128) * 2;
        uint32_t bVal = 255 - t / 2;
        red = 0; green = lum; blue = (uint8_t)((lum * bVal) >> 8);
      }
      break;
    }
    case 30: { // 30. 🎃 Spooky Witch Fire (Pumpkin Orange -> Toxic Slime Green -> Dark Violet)
      if (index256 < 85) {
        red = lum; green = (uint8_t)((lum * 100) >> 8); blue = 0; // Pumpkin Orange
      } else if (index256 < 170) {
        red = (uint8_t)((lum * 60) >> 8); green = lum; blue = 0; // Toxic Slime Green
      } else {
        red = (uint8_t)((lum * 160) >> 8); green = 0; blue = lum; // Dark Violet
      }
      break;
    }
    case 31: { // 31. 🎆 Fourth of July Triad (Laser Red -> Diamond White -> Royal Blue)
      if (index256 < 85) {
        red = lum; green = 0; blue = 0; // Red
      } else if (index256 < 170) {
        red = lum; green = lum; blue = lum; // White
      } else {
        red = 0; green = (uint8_t)((lum * 40) >> 8); blue = lum; // Blue
      }
      break;
    }
    case 32: { // 32. 🪙 Cyber Bronze & Copper (Burnt Umber -> Polished Copper -> Molten Gold)
      if (index256 < 128) {
        uint32_t rVal = 160 + (index256 * 7) / 10;
        uint32_t gVal = 60 + (index256 * 3) / 5;
        red = (uint8_t)((lum * rVal) >> 8); green = (uint8_t)((lum * gVal) >> 8); blue = (uint8_t)((lum * 20) >> 8);
      } else {
        uint32_t gVal = 137 + ((index256 - 128) * 4) / 5;
        uint32_t bVal = 20 + (index256 - 128) / 2;
        red = lum; green = (uint8_t)((lum * gVal) >> 8); blue = (uint8_t)((lum * bVal) >> 8);
      }
      break;
    }
    default: {
      hueToRgb(index256, lum, red, green, blue);
      break;
    }
  }
}

// Modular Palette + Motion Flow FX Engine (Supports all 16 Step 2 Motion Flows on True RGB & Palettes)
static inline void applyModularPaletteFX(uint8_t& red, uint8_t& green, uint8_t& blue, uint8_t palId, uint8_t motionId, unsigned long timeMs, uint8_t speed, uint8_t ledIndex = 0, uint16_t frameIndex = 0, uint16_t frameCount = 1, uint8_t ledCount = 55) {
  uint16_t safeFrameCount = (frameCount > 0) ? frameCount : 1;
  uint8_t safeLedCount = (ledCount > 0) ? ledCount : 55;
  uint8_t spd = (speed > 0) ? speed : 5;

  if (red == 0 && green == 0 && blue == 0) return;
  uint8_t lum = max(red, max(green, blue));
  if (lum == 0) return;

  uint8_t index256 = lum;

  switch (motionId) {
    case 0: { // 0. Static Tint
      index256 = lum;
      break;
    }
    case 1: { // 1. ⬆️ Flow UP (Geyser Outward to Tip)
      index256 = (uint8_t)(((timeMs * spd / 8) - (ledIndex * 256 / safeLedCount)) & 0xFF);
      break;
    }
    case 2: { // 2. ⬇️ Flow DOWN (Cascade Inward to Handle)
      index256 = (uint8_t)(((timeMs * spd / 8) + (ledIndex * 256 / safeLedCount)) & 0xFF);
      break;
    }
    case 3: { // 3. 🌧️ Matrix Pixel Rain (Falling Sparks)
      index256 = (uint8_t)(((timeMs * spd / 8) + (ledIndex * 14)) & 0xFF);
      break;
    }
    case 4: { // 4. 🌊 Dual Harmonic Tidal Surge (Up & Down Waves)
      int waveUp = (int)((timeMs * spd / 10) - (ledIndex * 8));
      int waveDown = (int)((timeMs * spd / 12) + (ledIndex * 8));
      index256 = (uint8_t)((waveUp ^ waveDown) & 0xFF);
      break;
    }
    case 5: { // 5. ⚡ Plasma Lightning Burst (Radial Pulse)
      index256 = (uint8_t)(((timeMs * spd / 6) - (ledIndex * 16)) & 0xFF);
      break;
    }
    case 6: { // 6. 💫 Stardust Meteor Cascade
      index256 = (uint8_t)(((timeMs * spd / 8) + (ledIndex * 12)) & 0xFF);
      break;
    }
    case 7: { // 7. 💓 Chroma Breathing Pulse
      index256 = (uint8_t)(((timeMs * spd / 14) + (ledIndex * 4)) & 0xFF);
      break;
    }
    case 8: { // 8. 🔄 Rotational POV Spin Sweep
      index256 = (uint8_t)(((timeMs * spd / 12) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      break;
    }
    case 9: { // 9. 🌀 3D Spiral Helix Vortex
      index256 = (uint8_t)(((timeMs * spd / 12) + (ledIndex * 6) + (frameIndex * 256 / safeFrameCount)) & 0xFF);
      break;
    }
    case 10: { // 10. ⚡ Hyper-Strobe Shimmer Blade
      bool strobe = ((frameIndex % 2) == 0) || (((timeMs / 30) % 2) == 0);
      if (!strobe) { red = (uint8_t)(red * 0.15f); green = (uint8_t)(green * 0.15f); blue = (uint8_t)(blue * 0.15f); return; }
      index256 = (uint8_t)((timeMs * spd / 14) & 0xFF);
      break;
    }
    case 11: { // 11. 🌌 Hyperspace Warp Shockwave
      uint32_t phase = (timeMs * spd / 6) & 0xFF;
      uint32_t dist = (ledIndex * 255) / safeLedCount;
      index256 = (uint8_t)((phase - ((dist * dist) >> 8)) & 0xFF);
      break;
    }
    case 12: { // 12. ⚡ Glitch Matrix Cyber Spark
      uint8_t noise = (uint8_t)(((ledIndex * 73) ^ (frameIndex * 151) ^ (timeMs >> 3)) & 0xFF);
      index256 = (uint8_t)(((timeMs * spd / 10) + noise) & 0xFF);
      break;
    }
    case 13: { // 13. 🌈 Rainbow Aurora Waveform
      int a1 = (int)((timeMs * spd / 10) + ledIndex * 7);
      int a2 = (int)((timeMs * spd / 16) - ledIndex * 5);
      index256 = (uint8_t)(((a1 + a2) / 2) & 0xFF);
      break;
    }
    case 14: { // 14. 🌋 Lava Flare
      uint8_t b1 = (uint8_t)((timeMs * spd / 8) + ledIndex * 16);
      uint8_t b2 = (uint8_t)((timeMs * spd / 12) - ledIndex * 20);
      index256 = (uint8_t)(max(b1, b2) & 0xFF);
      break;
    }
    case 15: { // 15. 🪩 Disco BPM Strobe Blast
      uint8_t beat = ((timeMs * spd / 20) % 8);
      bool isFlash = (beat == 0 || beat == 2 || (ledIndex % 4 == (beat / 2)));
      if (!isFlash) { red = (uint8_t)(red * 0.12f); green = (uint8_t)(green * 0.12f); blue = (uint8_t)(blue * 0.12f); return; }
      index256 = (uint8_t)((timeMs * spd / 10) & 0xFF);
      break;
    }
    case 16: { // 16. 🪐 Black Hole Gravitational Pull
      uint32_t phase = (timeMs * spd / 6) & 0xFF;
      uint32_t invDist = ((safeLedCount - 1 - ledIndex) * 255) / safeLedCount;
      index256 = (uint8_t)((phase + ((invDist * invDist) >> 8)) & 0xFF);
      break;
    }
  }

  // If True RGB (palId == 0), modulate pattern brightness & crests directly on original artwork!
  if (palId == 0) {
    if (motionId == 0) return;
    float factor = 0.30f + 0.70f * (index256 / 255.0f);
    red = (uint8_t)(red * factor);
    green = (uint8_t)(green * factor);
    blue = (uint8_t)(blue * factor);
    if (index256 > 230) {
      uint8_t crest = (uint8_t)((index256 - 230) * 8);
      red = min(255, (int)(red + crest));
      green = min(255, (int)(green + crest));
      blue = min(255, (int)(blue + crest));
    }
    return;
  }

  getPaletteBaseColor(palId, index256, lum, red, green, blue);
}


// ----------------------------------------------------
// 32px STRIP COLOR-EXTRACTION & HARMONIC FLOW ENGINE
// ----------------------------------------------------
#if defined(STRIP_32PX) || defined(PEBBLE_50PX)
struct ExtractedSlotPalette {
  uint8_t count;
  RgbColor colors[8];
};

static ExtractedSlotPalette g_slotPalette = {0};

static inline void extractColorsFromActivePattern(OpenPixelPoiConfig& config) {
  g_slotPalette.count = 0;
  if (!config.pattern || config.patternLength < 3) {
    g_slotPalette.colors[0] = RgbColor(0, 255, 136); // Neon Green
    g_slotPalette.colors[1] = RgbColor(0, 210, 255); // Neon Cyan
    g_slotPalette.count = 2;
    return;
  }

  size_t totalPixels = config.patternLength / 3;
  size_t step = max((size_t)1, totalPixels / 24);

  for (size_t i = 0; i < totalPixels && g_slotPalette.count < 8; i += step) {
    uint8_t r = config.pattern[i * 3 + 0];
    uint8_t g = config.pattern[i * 3 + 1];
    uint8_t b = config.pattern[i * 3 + 2];

    uint16_t bright = (uint16_t)r + (uint16_t)g + (uint16_t)b;
    if (bright >= 45) { // Filter dark/black noise
      bool isDuplicate = false;
      for (int k = 0; k < g_slotPalette.count; k++) {
        int dr = abs((int)r - (int)g_slotPalette.colors[k].R);
        int dg = abs((int)g - (int)g_slotPalette.colors[k].G);
        int db = abs((int)b - (int)g_slotPalette.colors[k].B);
        if (dr + dg + db < 65) {
          isDuplicate = true;
          break;
        }
      }
      if (!isDuplicate) {
        g_slotPalette.colors[g_slotPalette.count++] = RgbColor(r, g, b);
      }
    }
  }

  if (g_slotPalette.count == 0) {
    g_slotPalette.colors[0] = RgbColor(255, 0, 128); // Neon Pink
    g_slotPalette.colors[1] = RgbColor(0, 230, 255); // Neon Cyan
    g_slotPalette.count = 2;
  } else if (g_slotPalette.count == 1) {
    g_slotPalette.colors[1] = RgbColor(g_slotPalette.colors[0].B, g_slotPalette.colors[0].R, g_slotPalette.colors[0].G);
    g_slotPalette.count = 2;
  }
}
#endif

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
#if defined(PEBBLE_50PX)
      config.ledCount = 88;
      config.frameHeight = 88;
      config.motionFxMode = 0;
      if (config.frameCount < 2) config.frameCount = 30;
      ledStrip = new NeoPixelStrip(88, 10); // 88 WS2811/WS2812 LEDs on Pin D10 (GPIO 10) on the right side!
#elif defined(STRIP_32PX)
      config.ledCount = 32;
      config.frameHeight = 32;
      config.motionFxMode = 0;
      if (config.frameCount < 2) config.frameCount = 30;
      ledStrip = new DotStarStrip(32, 6, 5); // 32 LEDs on Pin 6 (Data) & Pin 5 (Clock)
#else
      config.ledCount = 55;
      config.frameHeight = 55;
      if (config.frameCount < 2) config.frameCount = 30;
      ledStrip = new DotStarStrip(55, 6, 7); // 55 LEDs on Pin 6 (Data) & Pin 7 (Clock)
#endif
      ledStrip->Begin();
      frameIndex = 0;
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

        uint16_t safeSpeed = (config.animationSpeed >= 1) ? config.animationSpeed : 30;
        uint16_t safeFCount = (config.frameCount >= 1) ? config.frameCount : 30;
        uint32_t frameIntervalMicros = max((uint32_t)500, (uint32_t)(1000000 / safeSpeed));
        frameIndex = ((micros() - (config.displayStateLastUpdated * 1000)) / frameIntervalMicros) % safeFCount;
        if(lastFrameIndex == frameIndex){
          return;
        }else{
          lastFrameIndex = frameIndex;
        }

        // Zero-RAM Transition Engine (strictly OFF when blendMode == 0)
      uint16_t currentDuration = getBlendDuration(config.blendMode);
        bool isTransitioning = (config.blendMode > 0 && config.blendStartTime > 0 && (millis() - config.blendStartTime < currentDuration));
        float tProgress = isTransitioning ? ((float)(millis() - config.blendStartTime) / (float)currentDuration) : 1.0f;


#if defined(STRIP_32PX) || defined(PEBBLE_50PX)
#ifdef STRIP_32PX
        // 🥷 STEALTH OFF BATTERY SAVER (Only for 32px auxiliary strip):
        if (config.motionFxMode == 0) {
          for (int j=0; j<config.ledCount; j++) {
            ledStrip->SetPixelColor(j, RgbColor(0, 0, 0));
          }
          ledStrip->Show();
          return;
        }
#endif

        // Extract palette when slot loads or changes
        static int lastLoadedSlot = -1;
        if (lastLoadedSlot != config.getActivePatternIndex()) {
          lastLoadedSlot = config.getActivePatternIndex();
          extractColorsFromActivePattern(config);
        }

        unsigned long now = millis();
        uint8_t spd = max((uint8_t)1, config.paletteSpeed);
        uint32_t effT = (now * (uint32_t)spd) / 4;
        uint8_t numC = max((uint8_t)2, g_slotPalette.count);

        uint8_t activeEffect = config.motionFxMode;
#ifdef PEBBLE_50PX
        if (activeEffect == 0) {
          // Normal Hat Mode: Slot (1-10 on Nunchuk) selects which crazy spaced-out ripple pattern plays!
          activeEffect = (config.getActivePatternIndex() % 16) + 1;
        }
#endif

        for (int j=0; j<config.ledCount; j++){
          uint8_t factor = 0;
          uint8_t colorPhase = 0;

          switch (activeEffect) {
            case 1: { // 1. ⬆️ Chasing Comet Pulses (Moving UP with Dark Gaps)
              uint8_t pos = (uint8_t)(((j * 256 / config.ledCount) - (effT / 3)) & 0xFF);
              uint8_t wave = fastSin8(pos * 3);
              factor = (wave > 150) ? (uint8_t)(((uint16_t)(wave - 150) * 255) / 105) : 0;
              colorPhase = (pos + (effT / 8)) & 0xFF;
              break;
            }
            case 2: { // 2. ⬇️ Cascading Rain Drops (Moving DOWN with Spaced Gaps)
              uint8_t pos = (uint8_t)(((j * 256 / config.ledCount) + (effT / 3)) & 0xFF);
              uint8_t wave = fastSin8(pos * 3);
              factor = (wave > 150) ? (uint8_t)(((uint16_t)(wave - 150) * 255) / 105) : 0;
              colorPhase = (pos - (effT / 8)) & 0xFF;
              break;
            }
            case 3: { // 3. 🌧️ Matrix Isolated Sparks
              uint8_t spark = fastSin8(j * 42 + (effT / 3));
              factor = (spark > 215) ? 255 : ((spark > 185) ? (uint8_t)((spark - 185) * 3) : 0);
              colorPhase = (j * 16 + (effT / 6)) & 0xFF;
              break;
            }
            case 4: { // 4. 🌊 Dual Passing Chaser Beams (Two Comet Heads Passing Each Other)
              uint8_t wave1 = fastSin8((j * 120 / config.ledCount) - (effT / 4));
              uint8_t wave2 = fastSin8((j * 120 / config.ledCount) + (effT / 5));
              uint8_t p1 = (wave1 > 170) ? (uint8_t)((wave1 - 170) * 3) : 0;
              uint8_t p2 = (wave2 > 170) ? (uint8_t)((wave2 - 170) * 3) : 0;
              factor = min(255, (int)(p1 + p2));
              colorPhase = (j * 12 + (effT / 6)) & 0xFF;
              break;
            }
            case 5: { // 5. ⚡ Laser Pulse Beads (Spaced Running Dots)
              uint8_t dot = ((j + (effT / 16)) % 7);
              factor = (dot == 0) ? 255 : ((dot == 1 || dot == 6) ? 70 : 0);
              colorPhase = (j * 24 + (effT / 8)) & 0xFF;
              break;
            }
            case 6: { // 6. 💫 Shooting Meteor Head with Fading Tail
              uint8_t mPos = (effT / 12) % (config.ledCount + 8);
              int dist = (int)mPos - (int)j;
              if (dist >= 0 && dist < 7) {
                factor = (uint8_t)(255 - dist * 36);
              } else {
                factor = 0;
              }
              colorPhase = (j * 16) & 0xFF;
              break;
            }
            case 7: { // 7. 💓 Breathing Center Glow Pulse
              int center = config.ledCount / 2;
              int dist = abs(j - center);
              uint8_t breath = fastSin8(effT / 6);
              uint8_t maxDist = (uint8_t)((breath * 10) / 255);
              factor = (dist <= maxDist) ? (uint8_t)(255 - dist * 24) : 0;
              colorPhase = (breath + j * 8) & 0xFF;
              break;
            }
            case 8: { // 8. 🔄 Traveling Spark Orbit
              uint8_t head = (effT / 14) % config.ledCount;
              int d = abs(j - (int)head);
              factor = (d <= 3) ? (uint8_t)(255 - d * 70) : 0;
              colorPhase = (j * 32) & 0xFF;
              break;
            }
            case 9: { // 9. 🌀 Dual Interleaved Beads (Opposite Direction Dots)
              bool isEven = (j % 2 == 0);
              uint8_t pos = isEven ? ((j + (effT / 14)) % 8) : ((j - (effT / 14) + 64) % 8);
              factor = (pos == 0) ? 255 : ((pos == 1) ? 60 : 0);
              colorPhase = (j * 20 + (effT / 10)) & 0xFF;
              break;
            }
            case 10: { // 10. ⚡ Hyper Strobe Sparks
              bool flash = ((effT / 40) % 4 == 0) && (j % 3 == 0);
              factor = flash ? 255 : 0;
              colorPhase = (effT / 10) & 0xFF;
              break;
            }
            case 11: { // 11. 🌌 Warp Shockwave Ring
              uint8_t ringPos = (effT / 10) % config.ledCount;
              int dist = abs(j - (int)ringPos);
              factor = (dist <= 2) ? (uint8_t)(255 - dist * 100) : 0;
              colorPhase = (ringPos * 8) & 0xFF;
              break;
            }
            case 12: { // 12. ⚡ Glitch Cyber Matrix (Random Spark Drops)
              uint8_t noise = (uint8_t)(((j * 73) ^ (effT / 8)) & 0xFF);
              factor = (noise > 225) ? 255 : ((noise > 195) ? 60 : 0);
              colorPhase = (j * 30) & 0xFF;
              break;
            }
            case 13: { // 13. 🌈 Aurora Spaced Ribbon
              uint8_t aur = fastSin8((j * 180 / config.ledCount) + (effT / 8));
              factor = (aur > 160) ? (uint8_t)((aur - 160) * 2.6f) : 0;
              colorPhase = (aur + (effT / 6)) & 0xFF;
              break;
            }
            case 14: { // 14. 🌋 Lava Ember Flares
              uint8_t flare = fastSin8(j * 40 + (effT / 5)) ^ fastSin8(j * 25 - (effT / 7));
              factor = (flare > 170) ? (uint8_t)((flare - 170) * 3) : 0;
              colorPhase = (flare + (effT / 10)) & 0xFF;
              break;
            }
            case 15: { // 15. 🪩 Disco Beat Strobe Beads
              uint8_t beat = (effT / 60) % 8;
              bool on = (j % 4 == (beat / 2)) && ((effT / 20) % 2 == 0);
              factor = on ? 255 : 0;
              colorPhase = (beat * 32) & 0xFF;
              break;
            }
            case 16: { // 16. 🕳️ Inward Gravity Convergence (Beads Rushing to Center)
              int center = config.ledCount / 2;
              int dist = abs(j - center);
              uint8_t rush = fastSin8(dist * 35 - (effT / 3));
              factor = (rush > 175) ? (uint8_t)((rush - 175) * 3) : 0;
              colorPhase = (dist * 20 + (effT / 8)) & 0xFF;
              break;
            }
            default: {
              uint8_t pos = (uint8_t)(((j * 256 / config.ledCount) - (effT / 3)) & 0xFF);
              uint8_t wave = fastSin8(pos * 3);
              factor = (wave > 150) ? (uint8_t)(((uint16_t)(wave - 150) * 255) / 105) : 0;
              colorPhase = (pos + (effT / 8)) & 0xFF;
              break;
            }
          }

          if (factor == 0) {
            ledStrip->SetPixelColor(j, RgbColor(0, 0, 0));
            continue;
          }

          uint8_t cIdx1 = (colorPhase * numC) / 256;
          uint8_t cIdx2 = (cIdx1 + 1) % numC;
          uint8_t frac = (colorPhase * numC) % 256;

          RgbColor c1 = g_slotPalette.colors[cIdx1];
          RgbColor c2 = g_slotPalette.colors[cIdx2];

          uint8_t r = (uint8_t)(((uint16_t)c1.R * (255 - frac) + (uint16_t)c2.R * frac) / 255);
          uint8_t g = (uint8_t)(((uint16_t)c1.G * (255 - frac) + (uint16_t)c2.G * frac) / 255);
          uint8_t b = (uint8_t)(((uint16_t)c1.B * (255 - frac) + (uint16_t)c2.B * frac) / 255);

          if (config.paletteFxMode > 0) {
            getPaletteBaseColor(config.paletteFxMode, colorPhase, 255, r, g, b);
          }

          red = (uint8_t)(((uint16_t)r * factor) / 255);
          green = (uint8_t)(((uint16_t)g * factor) / 255);
          blue = (uint8_t)(((uint16_t)b * factor) / 255);

          ledStrip->SetPixelColor(j, RgbColor(red, green, blue));
        }
#else
        const uint16_t frameStride = config.frameHeight * 3;
        uint32_t frameBase = (uint32_t)frameIndex * frameStride;

        for (int j=0; j<config.ledCount; j++){
          uint32_t pixelIdx = frameBase + (j * 3);
          red = config.pattern[pixelIdx + 0];
          green = config.pattern[pixelIdx + 1];
          blue = config.pattern[pixelIdx + 2];

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
          if (config.paletteFxMode > 0 || config.motionFxMode > 0) {
            applyModularPaletteFX(red, green, blue, config.paletteFxMode, config.motionFxMode, millis(), config.paletteSpeed, j, frameIndex, config.frameCount, config.ledCount);
          }

          ledStrip->SetPixelColor(config.ledCount-1-j, RgbColor(red, green, blue)); // Invert display for POV arc

        }
#endif
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
        // 5 Color-Coded Bank Zones across the strip (11 LEDs each on 55px blade):
        // Bank 1: 🟢 Neon Green (0x00, 0xFF, 0x55)
        // Bank 2: 🟡 Neon Yellow (0xFF, 0xD7, 0x00)
        // Bank 3: 🟣 Neon Purple (0xBC, 0x13, 0xFE)
        // Bank 4: 🌸 Neon Pink   (0xFF, 0x2E, 0x63)
        // Bank 5: 🩵 Neon Cyan   (0x00, 0xD2, 0xFF)
        const RgbColor bankColors[5] = {
          RgbColor(0x00, 0xFF, 0x55), // Bank 1: Neon Green
          RgbColor(0xFF, 0xD7, 0x00), // Bank 2: Neon Yellow
          RgbColor(0xBC, 0x13, 0xFE), // Bank 3: Neon Purple
          RgbColor(0xFF, 0x2E, 0x63), // Bank 4: Neon Pink
          RgbColor(0x00, 0xD2, 0xFF)  // Bank 5: Neon Cyan
        };

        int totalLeds = config.ledCount > 0 ? config.ledCount : 55;
        int zoneSize = max(1, totalLeds / 5);
        int activeBank = (config.patternBank < 5) ? config.patternBank : 0;

        for (int b = 0; b < 5; b++) {
          int startIdx = b * zoneSize;
          int endIdx = (b == 4) ? totalLeds : (b + 1) * zoneSize;
          bool isSelected = (b == activeBank);

          for (int k = startIdx; k < endIdx; k++) {
            if (isSelected) {
              // Active chosen bank zone pulses white and crisp bank color
              bool pulse = ((millis() / 80) % 2 == 0);
              if (pulse) {
                ledStrip->SetPixelColor(k, RgbColor(255, 255, 255));
              } else {
                ledStrip->SetPixelColor(k, bankColors[b]);
              }
            } else {
              // Non-selected banks are softly lit in their signature color
              RgbColor dimCol = RgbColor(bankColors[b].R / 6, bankColors[b].G / 6, bankColors[b].B / 6);
              ledStrip->SetPixelColor(k, dimCol);
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
        // Live Visual Palette Preview Menu while holding Click 5 (32 Palettes)
        int palIndex = ((millis() - config.displayStateLastUpdated) / 350) % 32 + 1;
        for (int j=0; j<config.ledCount; j++){
          uint8_t sampleLum = (uint8_t)((j * 255) / max(1, config.ledCount - 1));
          uint8_t r, g, b;
          getPaletteBaseColor(palIndex, sampleLum, 255, r, g, b);
          ledStrip->SetPixelColor(j, RgbColor(r, g, b));
        }

      }else if(config.displayState == DS_PALETTE_SELECT){
        // Dedicated Multi-Color Palette & Motion Preview across the full LED blade
        int totalLeds = config.ledCount > 0 ? config.ledCount : 55;
        if (config.paletteFxMode == 0) {
          // Stage 1 Blank Mode: Pure Original RGB Bitmap (Clean Diamond White Strobe Pulse)
          bool pulse = ((millis() / 120) % 2 == 0);
          for (int j = 0; j < totalLeds; j++) {
            if (pulse) {
              ledStrip->SetPixelColor(j, (j % 2 == 0) ? RgbColor(255, 255, 255) : RgbColor(120, 120, 120));
            } else {
              ledStrip->SetPixelColor(j, (j % 2 == 1) ? RgbColor(255, 255, 255) : RgbColor(60, 60, 60));
            }
          }
        } else if (config.motionFxMode == 0) {
          // Stage 1: Spread the full rich multi-color palette spectrum across all 55 LEDs!
          for (int j = 0; j < totalLeds; j++) {
            uint8_t stripIndex = (uint8_t)((j * 255) / max(1, totalLeds - 1));
            uint8_t r, g, b;
            getPaletteBaseColor(config.paletteFxMode, stripIndex, 255, r, g, b);
            ledStrip->SetPixelColor(j, RgbColor(r, g, b));
          }
        } else {
          // Stage 2: Animate the live chosen Motion Flow across the blade!
          for (int j = 0; j < totalLeds; j++) {
            uint8_t r = 255, g = 255, b = 255;
            applyModularPaletteFX(r, g, b, config.paletteFxMode, config.motionFxMode, millis(), config.paletteSpeed, j, 0, 1, totalLeds);
            ledStrip->SetPixelColor(j, RgbColor(r, g, b));
          }
        }
      }


// Battery 2-LED override removed to prevent false trigger

      // Output
      ledStrip->Show();
    }
};

#endif
