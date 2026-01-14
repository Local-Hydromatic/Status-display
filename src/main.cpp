#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "HT_lCMEN2R13EFC1.h"

#include "config.h"

#ifndef WIFI_SSID
#error "Please create include/config.h based on include/config.example.h"
#endif

#ifndef VEXT_PIN
#define VEXT_PIN 45
#endif

#ifndef EPD_SPI_FREQUENCY
#define EPD_SPI_FREQUENCY 6000000
#endif

HT_ICMEN2R13EFC1 display(
    EPD_RST_PIN,
    EPD_DC_PIN,
    EPD_CS_PIN,
    EPD_BUSY_PIN,
    EPD_SCK_PIN,
    EPD_MOSI_PIN,
    EPD_MISO_PIN,
    EPD_SPI_FREQUENCY);

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
uint16_t displayWidth = 0;
uint16_t displayHeight = 0;

constexpr uint8_t kFontSmallHeight = 13;
constexpr uint8_t kFontMediumHeight = 16;

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

DISPLAY_ANGLE displayAngle() {
  switch (DISPLAY_ROTATION) {
    case 0:
      return ANGLE_0_DEGREE;
    case 1:
      return ANGLE_90_DEGREE;
    case 2:
      return ANGLE_180_DEGREE;
    case 3:
      return ANGLE_270_DEGREE;
    default:
      return ANGLE_0_DEGREE;
  }
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

  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

  const int padding = 2;
  uint16_t textWidth = display.getStringWidth(text);
  int x = static_cast<int>(displayWidth) - static_cast<int>(textWidth) - padding * 2;
  if (x < 0) {
    x = 0;
  }
  int y = 0;
  int h = kFontSmallHeight + padding * 2;

  display.setColor(BLACK);
  display.fillRect(x, y, textWidth + padding * 2, h);
  display.setColor(WHITE);
  display.drawString(x + padding, y + padding, text);
  display.setColor(BLACK);
}

void drawHeader() {
  display.setFont(ArialMT_Plain_16);
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.drawString(0, 0, statusData.title.length() ? statusData.title : "System Status");
  drawStatusTag(statusData.status);

  display.setFont(ArialMT_Plain_10);
  display.drawString(0, kFontMediumHeight + 2, statusData.subtitle);
}

void drawDetails() {
  display.setFont(ArialMT_Plain_10);
  int y = kFontMediumHeight + kFontSmallHeight + 6;
  display.drawString(0, y, statusData.detail);
  y += kFontSmallHeight + 6;
  for (size_t i = 0; i < statusData.metricCount; ++i) {
    display.drawString(0, y, statusData.metrics[i].label + ": " + statusData.metrics[i].value);
    y += kFontSmallHeight + 2;
  }
}

void drawFooter() {
  display.setFont(ArialMT_Plain_10);
  String footer;
  if (statusData.updatedAt.length()) {
    footer = "Updated: " + statusData.updatedAt;
  } else if (lastMessageMillis > 0) {
    footer = "Last MQTT: " + formatUptimeSeconds((millis() - lastMessageMillis) / 1000);
  } else {
    footer = connectionStatus();
  }

  display.drawString(0, displayHeight - kFontSmallHeight, footer);
}

void renderDisplay() {
  display.clear();
  display.setColor(BLACK);
  drawHeader();
  drawDetails();
  drawFooter();
  display.update(BLACK_BUFFER);
  display.display();
}

void updateDisplayGeometry() {
  display.screenRotate(displayAngle());
  displayWidth = display.width();
  displayHeight = display.height();
}

void setVext(bool enabled) {
  pinMode(VEXT_PIN, OUTPUT);
  digitalWrite(VEXT_PIN, enabled ? LOW : HIGH);
}

void setup() {
  Serial.begin(115200);
  delay(200);

  setVext(true);
  delay(100);
  display.init();
  updateDisplayGeometry();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_LEFT);

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
