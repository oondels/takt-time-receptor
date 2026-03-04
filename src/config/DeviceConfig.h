#pragma once

#include <Arduino.h>

constexpr const char *DEFAULT_DEVICE_ID = "cost-default-id";
***REMOVED***
***REMOVED***
constexpr const char *DEFAULT_MQTT_SERVER = "10.57.25.135";
constexpr int DEFAULT_MQTT_PORT = 1883;
constexpr int DEFAULT_TAKT_COUNT = 0;
constexpr const char *DEFAULT_OTA_KEY = "default-ota-key";

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
