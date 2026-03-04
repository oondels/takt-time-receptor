#pragma once

#ifndef BUILD_WIFI_SSID
#define BUILD_WIFI_SSID "wifi-ssid-not-set"
#endif

#ifndef BUILD_WIFI_PASSWORD
#define BUILD_WIFI_PASSWORD "wifi-password-not-set"
#endif

#ifndef BUILD_DEFAULT_MQTT_USER
#define BUILD_DEFAULT_MQTT_USER "mqtt-user-not-set"
#endif

#ifndef BUILD_DEFAULT_MQTT_PASS
#define BUILD_DEFAULT_MQTT_PASS "mqtt-pass-not-set"
#endif

#ifndef BUILD_DEFAULT_MQTT_SERVER
#define BUILD_DEFAULT_MQTT_SERVER "mqtt-server-not-set"
#endif

#ifndef BUILD_DEFAULT_MQTT_PORT
#define BUILD_DEFAULT_MQTT_PORT 1883
#endif

#ifndef BUILD_DEFAULT_OTA_KEY
#define BUILD_DEFAULT_OTA_KEY "default-ota-key"
#endif

constexpr const char *DEFAULT_WIFI_SSID = BUILD_WIFI_SSID;
constexpr const char *DEFAULT_WIFI_PASSWORD = BUILD_WIFI_PASSWORD;
