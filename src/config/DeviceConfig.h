#pragma once

#include <Arduino.h>
#include "BuildEnv.h"

constexpr const char *DEFAULT_DEVICE_ID = "cost-default-id";
constexpr const char *DEFAULT_MQTT_USER = BUILD_DEFAULT_MQTT_USER;
constexpr const char *DEFAULT_MQTT_PASS = BUILD_DEFAULT_MQTT_PASS;
constexpr const char *DEFAULT_MQTT_SERVER = BUILD_DEFAULT_MQTT_SERVER;
constexpr int DEFAULT_MQTT_PORT = BUILD_DEFAULT_MQTT_PORT;
constexpr int DEFAULT_TAKT_COUNT = 0;
constexpr const char *DEFAULT_OTA_KEY = BUILD_DEFAULT_OTA_KEY;

struct DeviceConfig
{
  String deviceId;
  String mqttUser;
  String mqttPass;
  String mqttServer;
  int mqttPort;
  int taktCount;
  String otaKey;
};

void setDefaultConfig(DeviceConfig &cfg);
String buildMqttTopic(const DeviceConfig &cfg);
