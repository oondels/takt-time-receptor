#pragma once

#include <Arduino.h>

constexpr const char *DEFAULT_DEVICE_ID = "cost-default-id";
***REMOVED***
***REMOVED***
***REMOVED***
constexpr int DEFAULT_MQTT_PORT = 1883;
constexpr int DEFAULT_TAKT_COUNT = 0;

struct DeviceConfig
{
  String deviceId;
  String mqttUser;
  String mqttPass;
  String mqttServer;
  int mqttPort;
  int taktCount;
};

void setDefaultConfig(DeviceConfig &cfg);
String buildMqttTopic(const DeviceConfig &cfg);
