#pragma once

#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "power.h"

void uiInit();
void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery);
void uiPulse(const char *label);
void uiShowVolumeOverlay();

