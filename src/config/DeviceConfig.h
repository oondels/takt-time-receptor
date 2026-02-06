#pragma once

#include <Arduino.h>

constexpr const char *DEFAULT_DEVICE_ID = "takttike-default-id";
***REMOVED***
***REMOVED***
***REMOVED***
constexpr int DEFAULT_MQTT_PORT = 1883;

struct DeviceConfig
{
  String deviceId;
  String mqttUser;
  String mqttPass;
  String mqttServer;
  int mqttPort;
};

void setDefaultConfig(DeviceConfig &cfg);
String buildMqttTopic(const DeviceConfig &cfg);
