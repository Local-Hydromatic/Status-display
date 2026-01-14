# Status-display

Arduino sketch for the Heltec Wireless Paper (ESP32-S3 + 2.13" E-Ink) that subscribes to MQTT and renders a compact status dashboard.

## Features

- Wi-Fi + MQTT connectivity (Mosquitto compatible)
- JSON payload parsing with metrics support
- E-Paper UI sized for the 250 x 122 display
- Online/offline availability topic support

## Setup

1. **Install PlatformIO** in VS Code or via CLI.
2. Copy the configuration template and adjust credentials/pins (or share config from Hydromatic-Server as described below):
   ```bash
   cp include/config.example.h include/config.h
   ```
3. Update the MQTT broker, topic, and E-Paper pins in `include/config.h`. The defaults match Heltec's Wireless Paper reference sketch (RST=6, DC=5, CS=4, BUSY=7, SCK=3, MOSI=2, MISO=-1) and the Vext power control pin (GPIO45, active LOW). The project uses Heltec's `HT_lCMEN2R13EFC1` driver from the `Heltec_ESP32` library for the 2.13" E-Ink panel.
4. Build + upload:
   ```bash
   pio run -e heltec_wireless_paper
   pio run -e heltec_wireless_paper -t upload
   ```

### Sharing configuration with Hydromatic-Server

If you keep config, secrets, and MQTT settings in the Hydromatic-Server repo, you can share them here by adding the Hydromatic-Server `include/` directory to the build flags (already set in `platformio.ini`) and placing these files there:

- `config.h` (Wi-Fi + hardware settings)
- `secrets.h` (credentials/tokens)
- `mqtt.h` (MQTT broker + topics)

The firmware automatically includes those files when present, so both repos stay in sync.

> **Pin mapping:** Refer to the Heltec Wireless Paper schematic (linked on the product page) to confirm the E-Paper SPI pins and Vext control before flashing.

## MQTT Payload

Publish a JSON payload to the configured topic (default: `status/display`). Example:

```json
{
  "title": "Warehouse",
  "subtitle": "Line A",
  "status": "OK",
  "detail": "All systems nominal",
  "updated_at": "2025-01-21 08:42",
  "metrics": {
    "Temp": "22.4 C",
    "Hum": "35%",
    "Jobs": "184",
    "Alerts": "0"
  }
}
```

The display will show the header, status tag, details, metrics, and the last update time. When no payload has been received, it shows the connection status.

## Files

- `src/main.cpp` — main Arduino sketch
- `include/config.example.h` — configuration template
- `platformio.ini` — PlatformIO build configuration
- `.github/workflows/ci.yml` — GitHub Actions build

## Notes

If you need to match an existing data schema, update the JSON field names in `src/main.cpp` within `handlePayload()`.
