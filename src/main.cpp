#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include <GxEPD2_BW.h>
#include <GxEPD2_213_B73.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>

#include "config.h"

#ifndef WIFI_SSID
#error "Please create include/config.h based on include/config.example.h"
#endif

GxEPD2_BW<GxEPD2_213_B73, GxEPD2_213_B73::HEIGHT> display(
    GxEPD2_213_B73(EPD_CS_PIN, EPD_DC_PIN, EPD_RST_PIN, EPD_BUSY_PIN));

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

struct MetricItem {
  String label;
  String value;
};

struct StatusData {
  String title;
  String subtitle;
  String status;
  String detail;
  String updatedAt;
  MetricItem metrics[MAX_METRICS];
  size_t metricCount = 0;
  bool hasPayload = false;
};

StatusData statusData;
unsigned long lastDisplayRefresh = 0;
unsigned long lastMessageMillis = 0;

String connectionStatus() {
  if (WiFi.status() != WL_CONNECTED) {
    return "WiFi: connecting";
  }
  if (!mqttClient.connected()) {
    return "MQTT: reconnecting";
  }
  return "MQTT: online";
}

void ensureWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    delay(250);
  }
}

void publishAvailability(bool online) {
  if (String(MQTT_WILL_TOPIC).length() == 0) {
    return;
  }
  const char *payload = online ? MQTT_ONLINE_MESSAGE : MQTT_WILL_MESSAGE;
  mqttClient.publish(MQTT_WILL_TOPIC, payload, true);
}

bool ensureMQTT() {
  if (mqttClient.connected()) {
    return true;
  }

  String clientId = MQTT_CLIENT_ID;
  if (clientId.length() == 0) {
    clientId = String("wireless-paper-") + String((uint32_t)ESP.getEfuseMac(), HEX);
  }

  bool connected = false;
  if (String(MQTT_WILL_TOPIC).length() > 0) {
    connected = mqttClient.connect(
        clientId.c_str(),
        MQTT_USERNAME,
        MQTT_PASSWORD,
        MQTT_WILL_TOPIC,
        1,
        true,
        MQTT_WILL_MESSAGE);
  } else {
    connected = mqttClient.connect(clientId.c_str(), MQTT_USERNAME, MQTT_PASSWORD);
  }

  if (connected) {
    mqttClient.subscribe(MQTT_TOPIC);
    publishAvailability(true);
  }

  return connected;
}

String formatUptimeSeconds(unsigned long seconds) {
  unsigned long minutes = seconds / 60;
  unsigned long hours = minutes / 60;
  minutes %= 60;
  seconds %= 60;

  if (hours > 0) {
    return String(hours) + "h " + String(minutes) + "m";
  }
  if (minutes > 0) {
    return String(minutes) + "m " + String(seconds) + "s";
  }
  return String(seconds) + "s";
}

void updateMetrics(const JsonObject &metrics) {
  statusData.metricCount = 0;
  for (JsonPair pair : metrics) {
    if (statusData.metricCount >= MAX_METRICS) {
      break;
    }
    statusData.metrics[statusData.metricCount].label = pair.key().c_str();
    statusData.metrics[statusData.metricCount].value = pair.value().as<String>();
    statusData.metricCount++;
  }
}

void handlePayload(const JsonDocument &doc) {
  statusData.title = doc["title"] | "System Status";
  statusData.subtitle = doc["subtitle"] | "Heltec Wireless Paper";
  statusData.status = doc["status"] | "UNKNOWN";
  statusData.detail = doc["detail"] | doc["details"] | "Waiting for data";
  statusData.updatedAt = doc["updated_at"] | doc["timestamp"] | "";

  if (doc.containsKey("metrics") && doc["metrics"].is<JsonObject>()) {
    updateMetrics(doc["metrics"].as<JsonObject>());
  } else {
    statusData.metricCount = 0;
  }

  statusData.hasPayload = true;
  lastMessageMillis = millis();
}

void mqttCallback(char *topic, byte *payload, unsigned int length) {
  StaticJsonDocument<1024> doc;
  DeserializationError error = deserializeJson(doc, payload, length);
  if (error) {
    statusData.detail = String("JSON error: ") + error.c_str();
    statusData.hasPayload = false;
    lastMessageMillis = millis();
    return;
  }

  handlePayload(doc);
}

void drawStatusTag(const String &text) {
  if (text.length() == 0) {
    return;
  }

  display.setFont(&FreeSans9pt7b);
  int16_t tbx, tby;
  uint16_t tbw, tbh;
  display.getTextBounds(text, 0, 0, &tbx, &tby, &tbw, &tbh);

  const int padding = 4;
  int x = display.width() - tbw - padding * 2 - 4;
  int y = 2;
  int h = tbh + padding * 2;

  display.fillRect(x, y, tbw + padding * 2, h, GxEPD_BLACK);
  display.setTextColor(GxEPD_WHITE);
  display.setCursor(x + padding, y + h - padding);
  display.print(text);
  display.setTextColor(GxEPD_BLACK);
}

void drawHeader() {
  display.setFont(&FreeSansBold18pt7b);
  display.setCursor(0, 28);
  display.print(statusData.title.length() ? statusData.title : "System Status");
  drawStatusTag(statusData.status);

  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 46);
  display.print(statusData.subtitle);
}

void drawDetails() {
  display.setFont(&FreeSans9pt7b);
  display.setCursor(0, 68);
  display.print(statusData.detail);

  int y = 86;
  for (size_t i = 0; i < statusData.metricCount; ++i) {
    display.setCursor(0, y);
    display.print(statusData.metrics[i].label + ": " + statusData.metrics[i].value);
    y += 14;
  }
}

void drawFooter() {
  display.setFont(&FreeSans9pt7b);
  String footer;
  if (statusData.updatedAt.length()) {
    footer = "Updated: " + statusData.updatedAt;
  } else if (lastMessageMillis > 0) {
    footer = "Last MQTT: " + formatUptimeSeconds((millis() - lastMessageMillis) / 1000);
  } else {
    footer = connectionStatus();
  }

  display.setCursor(0, display.height() - 4);
  display.print(footer);
}

void renderDisplay() {
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
    drawHeader();
    drawDetails();
    drawFooter();
  } while (display.nextPage());
}

void setup() {
  Serial.begin(115200);
  delay(200);

  SPI.begin(EPD_SCK_PIN, EPD_MISO_PIN, EPD_MOSI_PIN, EPD_CS_PIN);
  display.init(115200);
  display.setRotation(DISPLAY_ROTATION);

  mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
  mqttClient.setCallback(mqttCallback);

  ensureWiFi();
  ensureMQTT();
  renderDisplay();
}

void loop() {
  ensureWiFi();
  ensureMQTT();
  mqttClient.loop();

  unsigned long now = millis();
  if (now - lastDisplayRefresh > DISPLAY_REFRESH_MS) {
    lastDisplayRefresh = now;
    renderDisplay();
  }
}
