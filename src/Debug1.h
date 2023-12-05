#include <stdlib.h>
#include <Arduino.h>
#include "Config1.h"

byte debugMode = DEBUG_ON;

#define DBG(...) debugMode == DEBUG_ON ? Serial.println(__VA_ARGS__) : NULL