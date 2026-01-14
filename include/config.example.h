#pragma once

// Wi-Fi settings
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASSWORD "YOUR_WIFI_PASSWORD"

// MQTT settings
#define MQTT_BROKER "192.168.1.50"
#define MQTT_PORT 1883
#define MQTT_USERNAME ""
#define MQTT_PASSWORD ""
#define MQTT_CLIENT_ID "heltec-wireless-paper"
#define MQTT_TOPIC "status/display"

// Optional availability topic for LWT (leave empty to disable)
#define MQTT_WILL_TOPIC "status/display/availability"
#define MQTT_WILL_MESSAGE "offline"
#define MQTT_ONLINE_MESSAGE "online"

// Heltec Wireless Paper V1.1 E-Ink pins (from Heltec reference sketch)
#define EPD_SCK_PIN 3
#define EPD_MOSI_PIN 2
#define EPD_MISO_PIN -1
#define EPD_CS_PIN 4
#define EPD_DC_PIN 5
#define EPD_RST_PIN 6
#define EPD_BUSY_PIN 7

// External power control (Vext) is active LOW on Wireless Paper
#define VEXT_PIN 45

// Display settings
#define DISPLAY_ROTATION 1
#define DISPLAY_REFRESH_MS 60000
#define MAX_METRICS 4
