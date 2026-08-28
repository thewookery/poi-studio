# 🏆 GOLD STANDARD FIRMWARE RELEASE (Build v152)

This directory contains the verified, rock-solid **Gold Standard** firmware snapshot for both the **Open Pixel Poi (55-LED DotStar)** and the **Master Wii Nunchuk BLE Bridge Transmitter**.

---

## 🌟 Key Features & Verified Stability:

### 1. 🟢 Open Pixel Poi (55-LED DotStar FX):
- **Hardware Architecture**: Seeed Studio XIAO ESP32-C3
- **LED Driver**: SK9822 / APA102 DotStar 55 LEDs on Data (Pin 6 / D4) & Clock (Pin 7 / D5)
- **Memory & Storage**: 50 Slots (5 Banks x 10 Slots) in LittleFS with dynamic file-size frame calculation (`frameCount = fileSize / 165`)
- **Boot Auto-Repair**: Hard-locked 55-pixel stride (165 bytes/frame) completely eliminating any upward shifting or 2-LED collapse glitches
- **Battery Safety**: Disabled false floating ADC readings on Pin A0 to prevent accidental low-voltage latching
- **Disconnect Auto-Recovery**: Snaps back to clean 55-pixel pattern playback immediately on BLE disconnect

### 2. 🔵 Master Wii Nunchuk BLE Bridge Transmitter:
- **Hardware Architecture**: Seeed Studio XIAO ESP32-C3 + Wii Nunchuk I2C (SDA Pin D4, SCL Pin D5) + Power Button (D1/D0)
- **Normal Joystick Navigation**:
  - Left / Right -> Pattern Slot (1–10)
  - Up / Down -> Hardware Bank (1–5)
- **C-Button**: Tap to cycle 32 Pro Palettes | Hold 1.0s to Reset True RGB
- **Z-Hold Step 2 Motion Remix**:
  - Hold Z + Joystick L/R -> Flick through all 16 Step 2 Motion Flows
  - Hold Z + Wrist Flick -> Instant random/next Motion Flow drop
  - Hold Z + Barrel Roll -> Smooth dynamic tilt speed throttle (3x to 10x overdrive)
  - Release Z -> Instant 0ms clean return to baseline pattern playback
- **Graceful Shutdown**: Sends clean reset commands and explicitly disconnects all BLE client sessions before entering deep sleep

---

## 📁 Snapshot Structure:
- `poi_firmware_src/`: Clean C++ source files for Poi
- `bridge_firmware_src/`: Clean C++ source files for Nunchuk Bridge
- `binaries/poi_55px_fx/`: Tested binary payload (bootloader, partitions, boot_app0, firmware.bin, littlefs.bin, manifest.json)
- `binaries/bridge_nunchuk/`: Tested binary payload (bootloader, partitions, boot_app0, firmware.bin, manifest.json)
