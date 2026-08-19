# Compiling Info
1. Connect your ESP32 PCB to your computer via USB, on windows it should show up as "USB Serial Device (COM#)" with the "#" being a random number.
1. Open this folder up in VSCode with the PlatformIO plugin stalled.
1. Hit the -> arrow button on the bottom bar to compile and upload the firmware to your PCB.

All the library dependencies and board config is contained in the platformio.ini file in this folder.

# Note: Export a compiled firmware to web-based firmware flasher.
1. Run `pio run` to compile the firmware binary.
2. Copy `.pio/build/seeed_xiao_esp32c3/firmware.bin` to `firmware/opp_firmware/custom/3.0.0-55px-fx/`, replacing the old one.
3. Reload Open POI Studio to flash the updated binary.
