#pragma once

#include <Arduino.h>
#include "config.h"

enum class PlaybackState {
  Stopped,
  Playing,
  Paused
};

struct AudioStatus {
  uint16_t track;
  uint8_t volume;
  PlaybackState state;
};

void audioInit();
void audioLoop();
void audioPlayTrack(uint16_t trackNumber);
void audioNext();
void audioPrev();
void audioTogglePause();
void audioVolumeUp();
void audioVolumeDown();
AudioStatus getAudioStatus();

