#pragma once

#include <Arduino.h>

constexpr const char *DEFAULT_DEVICE_ID = "cost-fab-cel";
constexpr const char *DEFAULT_MQTT_USER = "dass";
constexpr const char *DEFAULT_MQTT_PASS = "pHUWphISTl7r_Geis";
constexpr const char *DEFAULT_MQTT_SERVER = "10.100.1.43";
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
