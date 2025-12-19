# HomeSense AQI Sensor Firmware

This folder contains the ESP32 firmware for the HomeSense AQI monitoring project. It measures PM2.5, PM10, TVOC, Temperature, and Humidity, and syncs the data to a Supabase dashboard.

## 🚀 Key Features
- **GitHub OTA Updates:** Automatically check and download new firmware from GitHub releases.
- **Battery Smoothing:** Multi-sampling and EMA filters to prevent voltage percentage fluctuation (14-36% jumping fix).
- **Hardened Diagnostics:** Detects and reports restart reasons (Brownout, Watchdog, etc.) to the Serial Monitor.
- **Unified AQI:** Custom IAQ calculator that weights PM2.5, PM10, and TVOC data.

---

## 🛠 How to Create & Push a New Version
Follow these steps whenever you want to update your device remotely.

### Step 1: Update the Version in Code
1. Open `include/web_updater.h`.
2. Update the `VERSION` constant (e.g., from `"1.0.0"` to `"1.0.1"`):
   ```cpp
   static constexpr const char* VERSION = "1.0.1";
   ```

### Step 2: Build the Binary
1. Open your terminal in the `AQI_sensors` directory.
2. Run the PlatformIO build command:
   ```bash
   pio run
   ```
3. Your compiled binary will be located at:
   `.pio/build/esp32dev/firmware.bin`

### Step 3: Update `version.json`
Update the `version.json` file in the root of your GitHub repository:
```json
{
  "version": "1.0.1",
  "description": "Fixed battery smoothing and added GitHub OTA support.",
  "release_date": "2025-12-19"
}
```

### Step 4: Push to GitHub
Commit and push the updated `version.json` to your **main branch**:
```bash
git add version.json
git commit -m "Bump version to 1.0.1"
git push origin main
```

### Step 5: Create a GitHub Release
1. Go to your GitHub repository Online.
2. Click **Releases** -> **Create a new release**.
3. **Tag:** Enter `v1.0.1` (must start with `v` + version number).
4. **Title:** `Version 1.0.1`
5. **Attach Binary:** Upload the `firmware.bin` file from Step 2.
6. **⚠️ IMPORTANT:** Rename the file to **`firmware.ino.bin`** inside the GitHub upload box before publishing.
7. Click **Publish release**.

---

## ⚡ Hardware Troubleshooting
If the device restarts unexpectedly, check the Serial Monitor (115200 baud).
- **Reset Reason: ⚠️ BROWNOUT:** The battery is too low or cannot provide enough current for the PM Sensor fan.
- **Reset Reason: Task Watchdog:** The code got stuck in a loop for more than 30 seconds.
