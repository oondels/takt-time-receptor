#include "DeviceConfig.h"

void setDefaultConfig(DeviceConfig &cfg)
{
  cfg.deviceId = DEFAULT_DEVICE_ID;
  cfg.mqttUser = DEFAULT_MQTT_USER;
  cfg.mqttPass = DEFAULT_MQTT_PASS;
  cfg.mqttServer = DEFAULT_MQTT_SERVER;
  cfg.mqttPort = DEFAULT_MQTT_PORT;
}

String buildMqttTopic(const DeviceConfig &cfg)
{
  return String("takt/device/") + cfg.deviceId;
}
