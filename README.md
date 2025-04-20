# ESP32 BLETracker

A simple solution for tracking Bluetooth Low Energy devices with an ESP32, MQTT protocol, and Home Assistant integration. This project allows you to monitor BLE devices with public (non-changing) MAC addresses.

## Table of Contents

- [Overview](#overview)
- [Setup and Configuration](#setup-and-configuration)
  - [Battery Level Monitoring](#battery-level-monitoring)
- [Features](#features)
  - [Web Server](#web-server)
  - [MQTT Integration](#mqtt-integration)
  - [Home Assistant Integration](#home-assistant-integration)
  - [FHEM Support](#fhem-support)
- [Tested BLE Devices](#tested-ble-devices)
- [Troubleshooting](#troubleshooting)
  - [Reporting Crashes](#reporting-crashes)
- [Development Notes](#development-notes)
  - [Arduino IDE Notes](#arduino-ide-notes)
- [License](#license)
- [Support](#support)

## Overview

ESP32_BLETracker allows you to:
- Track BLE devices in your home or office
- Monitor device presence and battery levels
- Integrate with Home Assistant via MQTT
- Configure and monitor the system through a web interface

## Setup and Configuration

1. **Build and Deploy**:
   - Use [PlatformIO](https://platformio.org/) (recommended)
   - Install [git](https://git-scm.com/downloads) for automatic dependency management
   - Modify `user_config.h` with your WiFi and MQTT broker information

2. **First-time Setup**:
   - If WiFi credentials aren't provided, the ESP32 starts in access point mode
   - Connect to the access point (IP: 192.168.2.1)
   - Enter WiFi credentials through the web interface

3. **MQTT Configuration**:
   - Ensure your GATEWAY_NAME is unique (used as MQTT Client ID)
   - Configure MQTT broker details in the web interface or `user_config.h`

### Battery Level Monitoring

The system can read battery levels from BLE devices that support the Battery Service (0x180F) and Battery Level characteristic (0x2A19):

- Check device compatibility using a nRF Sniffer like [nRF Connect](https://play.google.com/store/apps/details?id=no.nordicsemi.android.mcp)
- Successfully tested with Nut Mini (other devices may have connection issues)
- For multiple devices, use a whitelist to prevent Wi-Fi disconnections:

```
BLE_BATTERY_WHITELIST       "XXXXXXXXX","YYYYYYYY"
```

**Important**: MAC addresses must be uppercase without ":" or "-" (e.g., "BA683F7EC159")

## Features

### Web Server

The integrated web server provides:
- System information dashboard
- Device configuration interface
- Log monitoring
- OTA (Over-The-Air) firmware updates

**Access Credentials**:
- Username: `admin`
- Password: `admin`

**Manual Scanning API**:
```
GET http://<user>:<password>@<device_ip>/scan?on=1
GET http://<user>:<password>@<device_ip>/scan?off=1
```

### MQTT Integration

The system publishes to the following topics:

1. **Availability Topic**:
   ```
   <LOCATION>/<GATEWAY_NAME>/LWT
   ```
   Payload: `online` or `offline`

2. **Device State** (two format options):
   - **JSON Format** (default):
     ```
     <LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>
     ```
     Payload: `{"state":"on|off","rssi":-XX,"battery":YY}`

   - **Separated Topics**:
     ```
     <LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/state
     <LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/rssi
     <LOCATION>/<GATEWAY_NAME>/<BLE_ADDRESS>/battery
     ```

3. **System Information**:
   ```
   <LOCATION>/<GATEWAY_NAME>/sysinfo
   ```
   Payload: `{"uptime":"...","version":"...","SSID":"...","IP":"..."}`

### Home Assistant Integration

Since version 3.8, the application supports automatic device discovery in Home Assistant.

#### MQTT Topics Used for Discovery

1. **Main tracker device topic**:
   ```
   homeassistant/sensor/{gateway_name}_status/config
   ```

2. **Device list sensor topic**:
   ```
   homeassistant/sensor/{gateway_name}_devices/config
   ```

3. **Individual BLE device topics**:
   ```
   homeassistant/device_tracker/{gateway_name}_device_{BLE_ADDRESS}/config
   ```

#### How to Remove Entities and Devices from Home Assistant

1. **Temporary removal**:
   - Disable the BLE Tracker or modify the configuration
   - Entities will become "unavailable" in Home Assistant

2. **Permanent removal**:
   - Publish an empty message to the discovery topics:
     ```
     homeassistant/device_tracker/{gateway_name}_device_{BLE_ADDRESS}/config
     ```
     with empty payload: `""`

3. **Manual removal from Home Assistant**:
   - Go to Settings > Devices & Services > MQTT
   - Find the "ESP32 BLETracker ({gateway_name})" device
   - Select "Remove" to delete the device and all its entities

4. **Completely disable Discovery**:
   - Modify `config.h`:
     ```cpp
     #define ENABLE_HOME_ASSISTANT_MQTT_DISCOVERY false
     ```
   - Recompile and upload the firmware

5. **Clean up the MQTT broker**:
   - Example with mosquitto_pub:
     ```bash
     mosquitto_pub -h [broker] -u [user] -P [password] -t "homeassistant/device_tracker/{gateway_name}_device_{BLE_ADDRESS}/config" -m ""
     ```

#### Manual Configuration

For older versions without auto-discovery, see the [example configuration](Doc/) in the Doc folder.

### FHEM Support

As an alternative to MQTT, the BLETracker can integrate with FHEM:
- Acts as a lepresenced daemon
- Listens on port 5333 for collectord connections
- Enable by choosing the **esp32dev-ble-fhem-release** build variant in PlatformIO
- Note: FHEM and MQTT support are mutually exclusive due to memory constraints

**Configuration Note**: The values of `presence_timeout` and `absence_timeout` in collectord.conf must be greater than the BLE_SCANNING_PERIOD (default: 10 seconds).

## Tested BLE Devices

| BLE Device            | Discovery | Battery |
|-----------------------|:---------:|:-------:|
| Nut mini              | ✔️        | ✔️      |
| Nut2                  | ✔️        | ❗️      |
| Remote Shutter        | ✔️        | ✔️      |
| Xiomi Amazfit Bip     | ✔️        | ❌      |
| REDMOND RFT-08S       | ✔️        | ❌      |
| Xiomi Mi Smart Band 4 | ✔️        | ❌      |
| Fitness Band GT101    | ✔️        | ❌      |
| Gigaset G-tag Beacon  | ✔️        | ✔️      |

## Troubleshooting

### Reporting Crashes

If you encounter a crash:

1. Build the firmware in debug mode (select the appropriate environment in PlatformIO)
2. Flash the firmware and open the console monitor
3. Reproduce the crash
4. Copy the backtrace from the console:
   ```
   Backtrace: 0x4008505f:0x3ffbe350 0x401b9df5:0x3ffbe370 ...
   ```
5. Save the backtrace to a file (e.g., backtrace.txt)
6. Run the decoder script:
   ```bash
   ./decoder.py -p ESP32 -t ~/.platformio/packages/toolchain-xtensa32 -e .pio/build/esp32dev-ble-debug/firmware.elf ./backtrace.txt
   ```
7. Open an issue with the decoder output and detailed reproduction steps

## Development Notes

### Arduino IDE Notes

While PlatformIO is recommended, you can build with Arduino IDE:

1. Install the ESP32 board in Arduino IDE ([instructions](https://randomnerdtutorials.com/installing-the-esp32-board-in-arduino-ide-windows-instructions/))
2. Install PubSubClient v2.8 library (not needed for FHEM support)
3. For FHEM support:
   - Install the [Regexp library](https://www.arduino.cc/reference/en/libraries/regexp/)
   - Set `USE_MQTT` to false and `USE_FHEM_LEPRESENCE_SERVER` to true in `config.h`
4. Replace the BLE library with the correct version:
   - For v2.1+: Use [this library](https://github.com/espressif/arduino-esp32/tree/6b0114366baf986c155e8173ab7c22bc0c5fcedc/libraries/BLE)
   - Library location:
     - Unix: `~/.arduino15/packages/esp32/hardware/esp32/x.x.x/libraries/BLE`
     - Windows: `C:\Users\YourUserName\AppData\Local\Arduino15\packages\esp32\hardware\esp32\x.x.x\libraries\BLE`

Build using the *Minimal SPIFFS* partition schema.

## License

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

## Support

If you like my work, please consider supporting me:

- [Buy me a coffee](https://www.buymeacoffee.com/shogunxam)
- [PayPal](https://paypal.me/shogunxam)

*If you find this project useful, please star the repository!*
