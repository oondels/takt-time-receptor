#pragma once

#include <ArduinoJson.h>

#include "DeviceConfig.h"

bool beginConfigStorage(bool formatOnFail = true);
bool loadConfig(DeviceConfig &cfg);
bool saveConfig(const DeviceConfig &cfg);
bool applyConfigFromJson(DeviceConfig &cfg, const JsonDocument &doc, bool &changed);
bool hasFirmwareSignatureChanged(const String &currentSignature);
bool saveFirmwareSignature(const String &signature);
