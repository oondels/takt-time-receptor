#pragma once

#include <Arduino.h>

#include "config/DeviceConfig.h"

void beginOtaServer(DeviceConfig &cfg);
void loopOtaServer();
