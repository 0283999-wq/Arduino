#include "audio.h"

#include <DFRobotDFPlayerMini.h>

static HardwareSerial dfSerial(1);
static DFRobotDFPlayerMini dfPlayer;

static uint16_t currentTrack = DEFAULT_TRACK;
static uint8_t currentVolume = DEFAULT_VOLUME;
static PlaybackState playbackState = PlaybackState::Stopped;
static bool initialized = false;
static unsigned long lastQuery = 0;

void audioInit() {
  dfSerial.begin(9600, SERIAL_8N1, PIN_DFPLAYER_TX, PIN_DFPLAYER_RX);
  if (dfPlayer.begin(dfSerial)) {
    initialized = true;
    dfPlayer.volume(currentVolume);
    audioPlayTrack(currentTrack);
  }
}

void audioLoop() {
  if (!initialized) return;
  unsigned long now = millis();
  if (now - lastQuery > 1000) {
    lastQuery = now;
    // Reserved for periodic polling
  }
}

void audioPlayTrack(uint16_t trackNumber) {
  currentTrack = trackNumber;
  if (initialized) {
    dfPlayer.playMp3Folder(currentTrack);
    playbackState = PlaybackState::Playing;
  }
}

void audioNext() {
  currentTrack++;
  audioPlayTrack(currentTrack);
}

void audioPrev() {
  if (currentTrack > 1) {
    currentTrack--;
  }
  audioPlayTrack(currentTrack);
}

void audioTogglePause() {
  if (!initialized) return;
  if (playbackState == PlaybackState::Playing) {
    dfPlayer.pause();
    playbackState = PlaybackState::Paused;
  } else {
    dfPlayer.start();
    playbackState = PlaybackState::Playing;
  }
}

void audioVolumeUp() {
  if (currentVolume < MAX_VOLUME) {
    currentVolume++;
    if (initialized) {
      dfPlayer.volume(currentVolume);
    }
  }
}

void audioVolumeDown() {
  if (currentVolume > MIN_VOLUME) {
    currentVolume--;
    if (initialized) {
      dfPlayer.volume(currentVolume);
    }
  }
}

AudioStatus getAudioStatus() {
  AudioStatus s{};
  s.track = currentTrack;
  s.volume = currentVolume;
  s.state = playbackState;
  return s;
}

