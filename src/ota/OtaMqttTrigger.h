#pragma once

#include <Arduino.h>

#include "config/DeviceConfig.h"

bool triggerOtaFromUrl(DeviceConfig &cfg, const String &updateUrl);
