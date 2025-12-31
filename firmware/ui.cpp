#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>
#include <math.h>

static Adafruit_GC9A01A display = Adafruit_GC9A01A(PIN_SCREEN_CS, PIN_SCREEN_DC, PIN_SCREEN_MOSI, PIN_SCREEN_SCK, PIN_SCREEN_RST);

struct UIStateCache {
  bool initialized = false;
  AudioStatus audio{};
  BatteryStatus battery{};
  UIMode mode = UIMode::DFP;
  uint16_t minuteTick = 0;
};

static UIStateCache uiCache;
static unsigned long lastHudRefresh = 0;
static unsigned long lastBatteryRefresh = 0;
static unsigned long lastBackgroundRefresh = 0;
static unsigned long lastPulse = 0;
static unsigned long volumeOverlayUntilMs = 0;
static bool backgroundDrawn = false;
static bool volumeOverlayActive = false;
static float overlayVolumeLerp = DEFAULT_VOLUME;
static uint8_t btAnimPhase = 0;
static unsigned long lastBtAnim = 0;

static void formatTime(char *buf, size_t len, unsigned long now) {
  unsigned long totalMinutes = (now / 60000UL) % (24UL * 60UL);
  uint8_t hours = totalMinutes / 60;
  uint8_t minutes = totalMinutes % 60;
  snprintf(buf, len, "%02u:%02u", hours, minutes);
}

static void drawSafeGridLineV(int16_t x) {
  int16_t dx = x - CENTER_X;
  if (abs(dx) > UI_SAFE_RADIUS) return;
  float span = sqrt((float)(UI_SAFE_RADIUS * UI_SAFE_RADIUS - dx * dx));
  int16_t y0 = CENTER_Y - (int16_t)span;
  int16_t y1 = CENTER_Y + (int16_t)span;
  display.drawFastVLine(x, y0, y1 - y0, COLOR_GRID);
}

static void drawSafeGridLineH(int16_t y) {
  int16_t dy = y - CENTER_Y;
  if (abs(dy) > UI_SAFE_RADIUS) return;
  float span = sqrt((float)(UI_SAFE_RADIUS * UI_SAFE_RADIUS - dy * dy));
  int16_t x0 = CENTER_X - (int16_t)span;
  int16_t x1 = CENTER_X + (int16_t)span;
  display.drawFastHLine(x0, y, x1 - x0, COLOR_GRID);
}

static void drawBackground() {
  display.fillScreen(COLOR_BG);
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS, COLOR_DARK);
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS - 2, COLOR_PANEL);

  for (int16_t x = UI_SAFE_LEFT + 10; x < UI_SAFE_LEFT + UI_SAFE_DIAMETER; x += 16) {
    drawSafeGridLineV(x);
  }
  for (int16_t y = UI_SAFE_TOP + 12; y < UI_SAFE_TOP + UI_SAFE_DIAMETER; y += 16) {
    drawSafeGridLineH(y);
  }

  for (uint16_t r = 18; r < UI_SAFE_RADIUS; r += 24) {
    display.drawCircle(CENTER_X, CENTER_Y, r, COLOR_DARK);
  }
}

static void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill) {
  display.fillRoundRect(x, y, w, h, 8, fill);
  display.drawRoundRect(x, y, w, h, 8, border);
}

static void drawTopBar(const BatteryStatus &bat, const char *timeStr, bool warn) {
  const int16_t x = UI_SAFE_LEFT + 10;
  const int16_t y = UI_SAFE_TOP + 8;
  const int16_t w = UI_SAFE_DIAMETER - 20;
  const int16_t h = 20;
  uint16_t border = warn ? COLOR_AMBER : COLOR_ACCENT;
  uint16_t fill = warn ? COLOR_WARNING : COLOR_PANEL;
  drawPanel(x, y, w, h, border, fill);

  display.setTextSize(1);
  display.setTextColor(warn ? COLOR_BG : COLOR_TEXT, fill);
  display.setCursor(x + 6, y + 6);
  if (warn) {
    display.print("WARNING!");
  } else {
    display.print("SYSTEM ONLINE");
  }

  display.setTextColor(COLOR_ACCENT, fill);
  int16_t timeX = x + w - 38;
  display.setCursor(timeX, y + 6);
  display.print(timeStr);
}

static void drawTrackPanel(const AudioStatus &audio, bool pulse) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 36;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 32;
  uint16_t border = pulse ? COLOR_AMBER : COLOR_ACCENT;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print("TRACK ");
  char buf[6];
  snprintf(buf, sizeof(buf), "%04d", audio.track);
  display.print(buf);

  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.setCursor(x + w - 54, y + 10);
  display.print("/");
  snprintf(buf, sizeof(buf), "%03d", audio.trackCount);
  display.print(buf);
}

static void drawStatePanel(const AudioStatus &audio, bool pulse) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 74;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 26;
  uint16_t border = pulse ? COLOR_AMBER : COLOR_ACCENT;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print(audio.state == PlaybackState::Playing ? "PLAY" : "PAUSE");

  display.setCursor(x + w - 70, y + 8);
  display.setTextColor(audio.online ? COLOR_ACCENT : COLOR_AMBER, COLOR_PANEL);
  display.print(audio.online ? "AUDIO CORE" : "AUDIO OFFLINE");
}

static void drawVolumePanel(const AudioStatus &audio) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 164;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 24;
  drawPanel(x, y, w, h, COLOR_ACCENT, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.setCursor(x + 10, y + 7);
  display.print("VOL");

  uint16_t barX = x + 36;
  uint16_t barY = y + 7;
  uint16_t barW = w - 60;
  uint16_t barH = h - 12;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map(audio.volume, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void drawBatteryPanel(const BatteryStatus &bat) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 118;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 32;
  drawPanel(x, y, w, h, COLOR_ACCENT, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print("BAT ");
  display.print(bat.voltage, 2);
  display.print("V  ");
  display.print(bat.percent);
  display.print("%");

  uint16_t iconX = x + w - 58;
  uint16_t iconY = y + 6;
  uint16_t iconW = 40;
  uint16_t iconH = h - 12;
  display.drawRoundRect(iconX, iconY, iconW, iconH, 3, COLOR_GRID);
  display.fillRect(iconX + iconW, iconY + 4, 4, iconH - 8, COLOR_GRID);

  uint16_t fill = map(bat.percent, 0, 100, 0, iconW - 4);
  uint16_t color = COLOR_AMBER;
  if (bat.level == BatteryLevel::Green) color = COLOR_TEXT;
  if (bat.level == BatteryLevel::Red) color = COLOR_WARNING;
  display.fillRoundRect(iconX + 2, iconY + 2, fill, iconH - 4, 2, color);
}

static void drawMessagePanel(const char *timeStr) {
  const int16_t x = UI_SAFE_LEFT + 14;
  const int16_t y = UI_SAFE_TOP + 100;
  const int16_t w = UI_SAFE_DIAMETER - 28;
  const int16_t h = 16;
  display.fillRoundRect(x, y, w, h, 6, COLOR_BG);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(x + 6, y + 4);
  display.print("TIME ");
  display.print(timeStr);
}

static void drawVolumeOverlayBar(uint8_t volume, float lerpValue) {
  const int16_t w = UI_SAFE_DIAMETER - 34;
  const int16_t h = 26;
  const int16_t x = UI_SAFE_LEFT + 17;
  const int16_t y = CENTER_Y - h / 2;

  display.fillRoundRect(x, y, w, h, 8, COLOR_BG);
  display.drawRoundRect(x, y, w, h, 8, COLOR_ACCENT);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(x + 10, y + 8);
  display.print("VOL ");
  display.print(volume);

  uint16_t barX = x + 58;
  uint16_t barY = y + 8;
  uint16_t barW = w - 72;
  uint16_t barH = 10;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map((uint16_t)lerpValue, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void updateVolumeOverlay(const AudioStatus &audio, unsigned long now) {
  if (!volumeOverlayActive) return;
  overlayVolumeLerp = overlayVolumeLerp + (audio.volume - overlayVolumeLerp) * 0.35f;
  drawVolumeOverlayBar(audio.volume, overlayVolumeLerp);
  if (now > volumeOverlayUntilMs) {
    volumeOverlayActive = false;
    drawVolumePanel(audio);
  }
}

static void drawBtHud(unsigned long now) {
  // Animated now-playing style bar
  const int16_t x = UI_SAFE_LEFT + 20;
  const int16_t y = UI_SAFE_TOP + 72;
  const int16_t w = UI_SAFE_DIAMETER - 40;
  const int16_t h = 32;
  drawPanel(x, y, w, h, COLOR_ACCENT, COLOR_PANEL);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 10);
  display.print("BLUETOOTH MODE");
  display.setCursor(x + 10, y + 20);
  display.print("LINK STANDBY");

  const int16_t barX = x + 10;
  const int16_t barY = y + h + 6;
  const int16_t barW = w - 20;
  const int16_t barH = 12;
  display.fillRoundRect(barX, barY, barW, barH, 4, COLOR_BG);
  display.drawRoundRect(barX, barY, barW, barH, 4, COLOR_ACCENT);

  if (now - lastBtAnim > 120) {
    lastBtAnim = now;
    btAnimPhase = (btAnimPhase + 1) % 12;
  }
  int16_t segW = (barW - 8) / 12;
  for (uint8_t i = 0; i < 12; ++i) {
    uint16_t color = (i == btAnimPhase) ? COLOR_TEXT : COLOR_GRID;
    display.fillRoundRect(barX + 4 + i * segW, barY + 2, segW - 2, barH - 4, 2, color);
  }
}

static bool audioChanged(const AudioStatus &a, const AudioStatus &b) {
  return a.track != b.track || a.volume != b.volume || a.state != b.state || a.online != b.online || a.trackCount != b.trackCount;
}

static bool batteryChanged(const BatteryStatus &a, const BatteryStatus &b) {
  return a.percent != b.percent || a.level != b.level || fabs(a.voltage - b.voltage) > 0.02f;
}

void uiInit() {
  display.begin();
  display.setRotation(SCREEN_ROTATION);
  display.setTextWrap(false);
  drawBackground();
  backgroundDrawn = true;
  lastBackgroundRefresh = millis();
  uiCache.initialized = false;
}

void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery, UIMode mode) {
  unsigned long now = millis();
  char timeBuf[6];
  formatTime(timeBuf, sizeof(timeBuf), now);
  uint16_t minuteTick = (now / 60000UL) % 1440UL;

  if (!backgroundDrawn || now - lastBackgroundRefresh > UI_BACKGROUND_REFRESH_MS) {
    drawBackground();
    backgroundDrawn = true;
    lastBackgroundRefresh = now;
    uiCache.initialized = false;
  }

  bool pulseActive = (now - lastPulse) <= UI_ANIM_MS;
  bool audioDiff = !uiCache.initialized || audioChanged(audio, uiCache.audio) || pulseActive;
  bool batDiff = !uiCache.initialized || batteryChanged(battery, uiCache.battery);
  bool modeDiff = !uiCache.initialized || mode != uiCache.mode;
  bool timeDiff = !uiCache.initialized || minuteTick != uiCache.minuteTick;

  if (modeDiff && mode == UIMode::BT) {
    volumeOverlayActive = false;
  }

  if (mode == UIMode::DFP) {
    bool warn = (battery.percent < UI_WARNING_THRESHOLD || battery.level == BatteryLevel::Red);
    if (batDiff || timeDiff || modeDiff) {
      drawTopBar(battery, timeBuf, warn);
    }

    if ((audioDiff || modeDiff || timeDiff) && (now - lastHudRefresh >= UI_HUD_REFRESH_MS)) {
      drawTrackPanel(audio, pulseActive);
      drawStatePanel(audio, pulseActive);
      drawMessagePanel(timeBuf);
      drawVolumePanel(audio);
      lastHudRefresh = now;
    }

    if (batDiff || (now - lastBatteryRefresh >= UI_BATTERY_REFRESH_MS)) {
      drawBatteryPanel(battery);
      lastBatteryRefresh = now;
    }

    updateVolumeOverlay(audio, now);
  } else {
    if (batDiff || timeDiff || modeDiff) {
      drawTopBar(battery, timeBuf, battery.percent < UI_WARNING_THRESHOLD || battery.level == BatteryLevel::Red);
      drawBatteryPanel(battery);
    }
    if (modeDiff || (now - lastBtAnim > 120)) {
      drawBtHud(now);
    }
  }

  uiCache.audio = audio;
  uiCache.battery = battery;
  uiCache.mode = mode;
  uiCache.minuteTick = minuteTick;
  uiCache.initialized = true;
}

void uiPulse(const char *label) {
  (void)label;
  lastPulse = millis();
}

void uiShowVolumeOverlay() {
  unsigned long now = millis();
  volumeOverlayActive = true;
  volumeOverlayUntilMs = now + UI_VOLUME_OVERLAY_MS;
  overlayVolumeLerp = uiCache.initialized ? uiCache.audio.volume : DEFAULT_VOLUME;
}
