#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "input.h"
#include "power.h"
#include "ui.h"

static unsigned long lastBatteryRead = 0;
static BatteryStatus cachedBattery{};

void setup() {
  Serial.begin(115200);
  powerInit();
  inputInit();
  audioInit();
  uiInit();
  cachedBattery = readBattery();
}

void loop() {
  InputEvent ev = inputPoll();
  switch (ev) {
    case InputEvent::PlayPause:
      audioTogglePause();
      uiPulse("PLAYBACK");
      break;
    case InputEvent::Next:
      audioNext();
      uiPulse("TRACK >>");
      break;
    case InputEvent::Prev:
      audioPrev();
      uiPulse("TRACK <<");
      break;
    case InputEvent::VolUp:
      if (audioVolumeUp()) {
        uiShowVolumeOverlay();
      }
      break;
    case InputEvent::VolDown:
      if (audioVolumeDown()) {
        uiShowVolumeOverlay();
      }
      break;
    default:
      break;
  }

  audioLoop();

  unsigned long now = millis();
  if (now - lastBatteryRead > 2000) {
    cachedBattery = readBattery();
    lastBatteryRead = now;
  }

  uiUpdate(getAudioStatus(), cachedBattery);
}

