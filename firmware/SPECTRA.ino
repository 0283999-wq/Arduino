#include <Arduino.h>
#include "config.h"
#include "audio.h"
#include "input.h"
#include "power.h"
#include "ui.h"

static unsigned long lastBatteryRead = 0;
static BatteryStatus cachedBattery{};
static UIMode currentMode = UIMode::DFP;

void setup() {
  Serial.begin(115200);
  Serial.println("=== SPECTRA SETUP START ===");
  powerInit();
  inputInit();
  audioInit();
  uiInit();
  cachedBattery = readBattery();
  Serial.println("=== SPECTRA SETUP END ===");
}

void loop() {
  InputEvent ev = inputPoll();
  switch (ev) {
    case InputEvent::PlayPause:
      if (currentMode == UIMode::DFP) {
        audioTogglePause();
        uiPulse("PLAYBACK");
      }
      break;
    case InputEvent::Next:
      if (currentMode == UIMode::DFP) {
        audioNext();
        uiPulse("TRACK >>");
      }
      break;
    case InputEvent::Prev:
      if (currentMode == UIMode::DFP) {
        audioPrev();
        uiPulse("TRACK <<");
      }
      break;
    case InputEvent::VolUp:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeUp()) {
          uiShowVolumeOverlay();
        }
      }
      break;
    case InputEvent::VolDown:
      if (currentMode == UIMode::DFP) {
        if (audioVolumeDown()) {
          uiShowVolumeOverlay();
        }
      }
      break;
    case InputEvent::ModeToggle:
      currentMode = (currentMode == UIMode::DFP) ? UIMode::BT : UIMode::DFP;
      uiPulse("MODE");
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

  uiUpdate(getAudioStatus(), cachedBattery, currentMode);
}

