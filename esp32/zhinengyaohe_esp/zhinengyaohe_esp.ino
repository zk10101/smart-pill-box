#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <stdio.h>
#include <string.h>

#if __has_include("local_config.h")
#include "local_config.h"
#endif

#ifndef PILL_WIFI_SSID
#define PILL_WIFI_SSID ""
#define PILL_WIFI_PASSWORD ""
#define PILL_MQTT_HOST ""
#define PILL_MQTT_PORT 1883
#define PILL_MQTT_USE_TLS 0
#define PILL_MQTT_CA_CERT ""
#define PILL_MQTT_CLIENT_ID ""
#define PILL_MQTT_USERNAME ""
#define PILL_MQTT_PASSWORD ""
#define PILL_MQTT_STATUS_TOPIC ""
#define PILL_MQTT_COMMAND_TOPIC ""
#endif

// U2 ESP32-C3 RXD0/TXD0 are wired to STM32 PA9/PA10 through 1k resistors.
static const int STM_RX_PIN = 20;
static const int STM_TX_PIN = 21;
static const uint32_t UART_BAUD = 115200;
static const uint32_t PING_PERIOD_MS = 3000;
static const uint32_t LINK_TIMEOUT_MS = 8000;
static const uint32_t NET_REPORT_MS = 3000;
static const uint32_t STATUS_QUERY_MS = 10000;
static const uint32_t MQTT_RETRY_MS = 5000;
static const uint32_t WIFI_RETRY_MS = 10000;

static const uint8_t CMD_GET_STATUS = 0x02;
static const uint8_t CMD_LINK_PING = 0x03;
static const uint8_t CMD_NET_STATUS = 0x19;
static const uint8_t CMD_ACK = 0x80;
static const uint8_t CMD_STATUS = 0x82;
static const uint8_t CMD_EVENT = 0x85;
static const uint8_t CMD_LINK_PONG = 0x86;
static const uint8_t LINK_PROTO_VERSION = 1;

WiFiClient plainClient;
WiFiClientSecure secureClient;
PubSubClient mqtt(plainClient);

struct RxFrame {
  uint8_t state = 0;
  uint8_t cmd = 0;
  uint8_t length = 0;
  uint8_t index = 0;
  uint8_t sum = 0;
  uint8_t data[128] = {};
  uint32_t lastByteMs = 0;
};

RxFrame rx;
uint8_t nextNonce = 1;
uint8_t waitingNonce = 0;
bool pingPending = false;
bool pongSeen = false;
uint32_t lastPongMs = 0;
uint32_t lastPingMs = 0;
uint32_t lastNetReportMs = 0;
uint32_t lastStatusQueryMs = 0;
uint32_t lastMqttAttemptMs = 0;
uint32_t lastWifiAttemptMs = 0;
uint32_t lastPublishMs = 0;
uint32_t stmUptimeMs = 0;
uint8_t stmRxOverflow = 0;
uint8_t stmState = 0xFF;
uint8_t stmError = 0xFF;
uint8_t stmCurrentSlot = 0;
uint8_t stmTargetSlot = 0;
bool stmSlotValid = false;
bool statusSeen = false;
bool statusDirty = true;
bool lastLinkOnline = false;

bool stmOnline() {
  return pongSeen && (uint32_t)(millis() - lastPongMs) < LINK_TIMEOUT_MS;
}

bool wifiConfigured() {
  return PILL_WIFI_SSID[0] != '\0';
}

bool mqttConfigured() {
  return PILL_MQTT_HOST[0] != '\0' && PILL_MQTT_CLIENT_ID[0] != '\0' &&
         PILL_MQTT_STATUS_TOPIC[0] != '\0' &&
         (!PILL_MQTT_USE_TLS || PILL_MQTT_CA_CERT[0] != '\0');
}

void sendFrame(uint8_t cmd, const uint8_t *data, uint8_t length) {
  uint8_t sum = (uint8_t)(cmd + length);
  Serial1.write((uint8_t)0xAA);
  Serial1.write((uint8_t)0x55);
  Serial1.write(cmd);
  Serial1.write(length);
  for (uint8_t i = 0; i < length; ++i) {
    Serial1.write(data[i]);
    sum = (uint8_t)(sum + data[i]);
  }
  Serial1.write(sum);
}

void publishStatus() {
  if (!mqtt.connected()) return;
  char payload[256];
  bool validStatus = stmOnline() && statusSeen;
  int length = snprintf(payload, sizeof(payload),
      "{\"stm32_online\":%s,\"wifi_online\":%s,\"mqtt_online\":true,"
      "\"stm32_uptime_ms\":%lu,\"stm32_rx_overflow\":%u,"
      "\"state\":%u,\"error\":%u,\"current_slot\":%u,\"target_slot\":%u,"
      "\"slot_valid\":%s}",
      stmOnline() ? "true" : "false", WiFi.status() == WL_CONNECTED ? "true" : "false",
      (unsigned long)stmUptimeMs, stmRxOverflow,
      validStatus ? stmState : 255U, validStatus ? stmError : 255U,
      validStatus ? stmCurrentSlot : 0U, validStatus ? stmTargetSlot : 0U,
      validStatus && stmSlotValid ? "true" : "false");
  if (length > 0 && length < (int)sizeof(payload)) {
    statusDirty = !mqtt.publish(PILL_MQTT_STATUS_TOPIC, payload);
  }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length) {
  (void)topic;
  // Cloud motion commands wait for a jointly agreed schema. Read-only query only.
  static const char command[] = "GET_STATUS";
  if (length == sizeof(command) - 1 &&
      memcmp(payload, command, sizeof(command) - 1) == 0) {
    sendFrame(CMD_GET_STATUS, nullptr, 0);
  }
}

void handleFrame() {
  switch (rx.cmd) {
    case CMD_LINK_PONG:
      if (rx.length == 7 && pingPending && rx.data[0] == waitingNonce &&
          rx.data[1] == LINK_PROTO_VERSION) {
        stmUptimeMs = (uint32_t)rx.data[2] |
            ((uint32_t)rx.data[3] << 8) | ((uint32_t)rx.data[4] << 16) |
            ((uint32_t)rx.data[5] << 24);
        stmRxOverflow = rx.data[6];
        pingPending = false;
        pongSeen = true;
        lastPongMs = millis();
      }
      break;
    case CMD_STATUS:
      if (rx.length == 14) {
        stmState = rx.data[0];
        stmError = rx.data[1];
        stmCurrentSlot = rx.data[3];
        stmTargetSlot = rx.data[12];
        stmSlotValid = rx.data[13] != 0;
        statusSeen = true;
        statusDirty = true;
      }
      break;
    case CMD_EVENT:
      if (rx.length == 3) {
        sendFrame(CMD_GET_STATUS, nullptr, 0);
      }
      break;
    case CMD_ACK:
      break;
    default:
      break;
  }
}

void feedByte(uint8_t value) {
  rx.lastByteMs = millis();
  switch (rx.state) {
    case 0:
      if (value == 0xAA) rx.state = 1;
      break;
    case 1:
      if (value == 0x55) rx.state = 2;
      else rx.state = (value == 0xAA) ? 1 : 0;
      break;
    case 2:
      rx.cmd = value;
      rx.sum = value;
      rx.state = 3;
      break;
    case 3:
      rx.length = value;
      rx.sum = (uint8_t)(rx.sum + value);
      rx.index = 0;
      rx.state = value > sizeof(rx.data) ? 0 : (value == 0 ? 5 : 4);
      break;
    case 4:
      rx.data[rx.index++] = value;
      rx.sum = (uint8_t)(rx.sum + value);
      if (rx.index == rx.length) rx.state = 5;
      break;
    case 5:
      if (value == rx.sum) handleFrame();
      rx.state = 0;
      break;
  }
}

void pollSerial() {
  while (Serial1.available() > 0) feedByte((uint8_t)Serial1.read());
  if (rx.state != 0 && (uint32_t)(millis() - rx.lastByteMs) > 200) rx.state = 0;
}

void maintainWifi() {
  if (!wifiConfigured() || WiFi.status() == WL_CONNECTED) return;
  if ((uint32_t)(millis() - lastWifiAttemptMs) < WIFI_RETRY_MS) return;
  lastWifiAttemptMs = millis();
  WiFi.begin(PILL_WIFI_SSID, PILL_WIFI_PASSWORD);
}

void maintainMqtt() {
  if (!mqttConfigured() || WiFi.status() != WL_CONNECTED) return;
  if (mqtt.connected()) {
    mqtt.loop();
    return;
  }
  if ((uint32_t)(millis() - lastMqttAttemptMs) < MQTT_RETRY_MS) return;
  lastMqttAttemptMs = millis();
  if (mqtt.connect(PILL_MQTT_CLIENT_ID, PILL_MQTT_USERNAME, PILL_MQTT_PASSWORD)) {
    if (PILL_MQTT_COMMAND_TOPIC[0] != '\0') mqtt.subscribe(PILL_MQTT_COMMAND_TOPIC);
    statusDirty = true;
  }
}

void setup() {
  Serial1.begin(UART_BAUD, SERIAL_8N1, STM_RX_PIN, STM_TX_PIN);
  WiFi.mode(WIFI_STA);
  if (wifiConfigured()) {
    WiFi.begin(PILL_WIFI_SSID, PILL_WIFI_PASSWORD);
    lastWifiAttemptMs = millis();
  }
  if (PILL_MQTT_USE_TLS) {
    secureClient.setCACert(PILL_MQTT_CA_CERT);
    mqtt.setClient(secureClient);
  }
  mqtt.setServer(PILL_MQTT_HOST, PILL_MQTT_PORT);
  mqtt.setBufferSize(512);
  mqtt.setCallback(onMqttMessage);

  lastPingMs = millis() - PING_PERIOD_MS;
  lastNetReportMs = millis() - NET_REPORT_MS;
  lastStatusQueryMs = millis() - STATUS_QUERY_MS;
}

void loop() {
  pollSerial();
  maintainWifi();
  maintainMqtt();
  uint32_t now = millis();
  bool linkOnline = stmOnline();
  if (linkOnline != lastLinkOnline) {
    lastLinkOnline = linkOnline;
    statusDirty = true;
  }

  if ((uint32_t)(now - lastPingMs) >= PING_PERIOD_MS) {
    lastPingMs = now;
    waitingNonce = nextNonce++;
    pingPending = true;
    sendFrame(CMD_LINK_PING, &waitingNonce, 1);
  }
  if ((uint32_t)(now - lastNetReportMs) >= NET_REPORT_MS) {
    uint8_t flags = (uint8_t)((WiFi.status() == WL_CONNECTED ? 1 : 0) |
                              (mqtt.connected() ? 2 : 0));
    lastNetReportMs = now;
    sendFrame(CMD_NET_STATUS, &flags, 1);
  }
  if (stmOnline() && (uint32_t)(now - lastStatusQueryMs) >= STATUS_QUERY_MS) {
    lastStatusQueryMs = now;
    sendFrame(CMD_GET_STATUS, nullptr, 0);
  }
  if (mqtt.connected() &&
      (statusDirty || (uint32_t)(now - lastPublishMs) >= 10000)) {
    lastPublishMs = now;
    publishStatus();
  }
}
