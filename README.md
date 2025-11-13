# ESP Pixhawk Arm (ESP32/ESP8266)

A tiny web UI and HTTP API to ARM/DISARM a Pixhawk-compatible autopilot via MAVLink using an ESP32 or ESP8266.

- Single sketch for both platforms with conditional compilation
- Async web stack with Server-Sent Events for instant status updates
- Wi-Fi provisioning via WiFiManager, mDNS, and OTA updates
- Flight mode control (RTL, Stabilize, Alt Hold)
- MAVLink command acknowledgment feedback

## Features
- Web UI (served from flash) with ARM/DISARM buttons and flight mode controls
- API:
  - `GET /status` → `{ "armed": bool, "mode": number }`
  - `POST /arm` → `{ "ok": true }`
  - `POST /disarm` → `{ "ok": true }`
  - `POST /mode/rtl` → `{ "ok": true }` (Return to Launch)
  - `POST /mode/stabilize` → `{ "ok": true }` (Stabilize mode)
  - `POST /mode/alt_hold` → `{ "ok": true }` (Altitude Hold)
  - SSE `GET /events` → events `status` and `ack` with command feedback
- Reads ARM state from MAVLink `HEARTBEAT` (no optimistic toggles)
- Displays MAVLink command acknowledgments in UI
- Works on ESP32 (HardwareSerial2) and ESP8266 (SoftwareSerial)
- Optional AP-only mode with fixed IP (172.217.28.1)

## Hardware
- Pixhawk serial telemetry → ESP UART
- Default pins:
  - ESP32: `RX=GPIO16`, `TX=GPIO17` (UART2)
  - ESP8266: `RX=GPIO5`, `TX=GPIO4` (SoftwareSerial)
- Override at compile time:
  - `-DMAV_RX_PIN=<gpio> -DMAV_TX_PIN=<gpio>`

## Dependencies
Install via Arduino Library Manager or PlatformIO:
- `ESPAsyncWebServer`
- `AsyncTCP` (ESP32) or `ESPAsyncTCP` (ESP8266)
- `WiFiManager` (tzapu)
- `ArduinoOTA`
- `mDNS` (`ESPmDNS` on ESP32, `ESP8266mDNS` on ESP8266)
- `mavlink` (Arduino MAVLink headers)

## Build
- Open `esp-pixhawk-arm.ino` in Arduino IDE
- Select board (ESP32 or ESP8266)
- Ensure dependencies are installed
- Flash; on first boot WiFiManager captive portal appears (SSID like `esp32-pixhawk` or `esp8266-pixhawk`)

### AP-only mode (optional)
Uncomment `#define AP_ONLY_MODE` in the sketch to enable access-point-only mode:
- Fixed IP: `172.217.28.1`
- AP SSID: `pixhawk32-ap` or `pixhawk8266-ap`
- Password: `12345678`

## Usage
- Open the device in a browser:
  - ESP32: `http://pixhawk32.local/` (or `http://172.217.28.1/` in AP-only mode)
  - ESP8266: `http://pixhawk8266.local/` (or `http://172.217.28.1/` in AP-only mode)
- Use UI buttons to ARM/DISARM or change flight modes
- Command acknowledgments appear as temporary messages in the UI
- All flight mode changes and ARM/DISARM commands are confirmed via MAVLink

## Security
- Endpoints are open by default. For production, add basic auth or a PIN check to POST `/arm` and `/disarm` handlers.

## Notes
- On ESP8266, SoftwareSerial is used; for robust telemetry consider ESP32 with HardwareSerial.
- UI is embedded and served from program memory; no filesystem required.

## License
This project is licensed under the MIT License (see `LICENSE`).
