#pragma once

#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "power.h"

enum class UIMode {
  DFP,
  BT
};

void uiInit();
void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery, UIMode mode);
void uiPulse(const char *label);
void uiShowVolumeOverlay();

