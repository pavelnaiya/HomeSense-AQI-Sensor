#!/bin/bash

# ESP32 Upload Script (Standard Safe Settings)
ESPTOOL_PY="/Users/pavelnaiya/.platformio/packages/tool-esptoolpy/esptool.py"

echo "================================================"
echo "ESP32 Manual Upload Script (Standard Mode)"
echo "================================================"
echo "1. Unplug USB"
echo "2. Hold BOOT"
echo "3. Plug USB (Hold BOOT)"
echo "4. Release BOOT"
echo "5. Press ENTER"
echo "================================================"
read -p "Ready?"

python3 "$ESPTOOL_PY" \
  --chip esp32 \
  --port /dev/cu.usbserial-0001 \
  --baud 115200 \
  --before no_reset \
  --after hard_reset \
  write_flash \
  -z \
  --flash_mode dout \
  --flash_freq 40m \
  --flash_size 4MB \
  0x1000 .pio/build/esp32dev/bootloader.bin \
  0x8000 .pio/build/esp32dev/partitions.bin \
  0x10000 .pio/build/esp32dev/firmware.bin
