#include <stdlib.h>
#include <Arduino.h>
#include "Config.h"

byte debugMode = DEBUG_ON;

#define DBG(...) debugMode == DEBUG_ON ? Serial.println(__VA_ARGS__) : NULL