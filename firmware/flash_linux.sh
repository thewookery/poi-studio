#!/usr/bin/env bash
# ==============================================================================
# Open POI Studio - Universal Linux 1-Click Flasher
# Supports: Ubuntu, Debian, Arch, Fedora, Raspberry Pi OS, Linux Mint
# ==============================================================================

set -e

echo ""
echo "======================================================================"
echo "⚡ Open POI Studio - Universal Linux Firmware Flasher"
echo "======================================================================"
echo ""

# 1. Check Python and esptool
if ! command -v esptool.py &> /dev/null; then
    echo "⚠️  esptool.py not found. Installing esptool via pip..."
    if command -v pip3 &> /dev/null; then
        pip3 install --user esptool
    elif command -v pip &> /dev/null; then
        pip install --user esptool
    else
        echo "❌ Error: python3-pip is required. Run: sudo apt install python3-pip (or distro equivalent)"
        exit 1
    fi
fi

# 2. Check / Setup udev permissions
if [ ! -f /etc/udev/rules.d/99-esp32.rules ]; then
    echo "🔧 Setting up Linux udev rules for Espressif ESP32-C3..."
    echo 'SUBSYSTEM=="usb", ATTR{idVendor}=="303a", MODE="0666", GROUP="plugdev"' | sudo tee /etc/udev/rules.d/99-esp32.rules > /dev/null
    echo 'KERNEL=="ttyACM*", MODE="0666", GROUP="dialout"' | sudo tee -a /etc/udev/rules.d/99-esp32.rules > /dev/null
    echo 'KERNEL=="ttyUSB*", MODE="0666", GROUP="dialout"' | sudo tee -a /etc/udev/rules.d/99-esp32.rules > /dev/null
    sudo udevadm control --reload-rules
    sudo udevadm trigger
    echo "✅ udev rules installed!"
fi

# 3. Detect Connected ESP32 Port
PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)

if [ -z "$PORT" ]; then
    echo "⚠️  No ESP32 detected on /dev/ttyACM* or /dev/ttyUSB*."
    echo "👉 Please plug in your ESP32-C3 via USB cable."
    echo "   (If it doesn't show up: hold 'B' button, press 'R' button, then release 'B')."
    echo ""
    read -p "Press Enter once connected..."
    PORT=$(ls /dev/ttyACM* /dev/ttyUSB* 2>/dev/null | head -n 1 || true)
    if [ -z "$PORT" ]; then
        echo "❌ Still no device detected. Check permissions with: sudo chmod 666 /dev/ttyACM*"
        exit 1
    fi
fi

echo "🔌 Detected ESP32 on port: $PORT"
echo ""
echo "Select Firmware to Flash:"
echo "  1) 🌟 55px DotStar Custom FX (50 Slots • 32 Palettes • 17 Motion FX) [Default]"
echo "  2) 🪩 Pure ESP-NOW Prop Receiver (55px DotStar • Zero-BLE Mesh)"
echo "  3) 🕹️ Master Wii Nunchuk ESP-NOW Bridge Transmitter (Xiao ESP32-C3)"
echo "  4) 🎯 55px DotStar Official Stock (3 Banks / 15 Slots)"
echo ""
read -p "Enter choice [1-4] (default: 1): " CHOICE
CHOICE=${CHOICE:-1}

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "$CHOICE" in
    1)
        FW_DIR="$DIR/opp_firmware/custom/3.0.0-55px-fx"
        echo "⚡ Flashing 55px DotStar Custom FX..."
        ;;
    2)
        FW_DIR="$DIR/opp_firmware/custom/3.0.0-55px-espnow"
        echo "⚡ Flashing Pure ESP-NOW Prop Receiver..."
        ;;
    3)
        FW_DIR="$DIR/opp_firmware/custom/1.0.0-bridge-nunchuk"
        echo "⚡ Flashing Master Wii Nunchuk Bridge..."
        ;;
    4)
        FW_DIR="$DIR/opp_firmware/app_2.0/3.0.0-55px"
        echo "⚡ Flashing 55px DotStar Stock..."
        ;;
    *)
        echo "❌ Invalid choice."
        exit 1
        ;;
esac

esptool.py --chip esp32c3 --port "$PORT" --baud 921600 write_flash \
    0x0000 "$FW_DIR/bootloader.bin" \
    0x8000 "$FW_DIR/partitions.bin" \
    0x10000 "$FW_DIR/firmware.bin"

echo ""
echo "======================================================================"
echo "🎉 SUCCESS: Firmware successfully flashed on Linux!"
echo "======================================================================"
echo "Unplug USB cable and enjoy spinning!"
