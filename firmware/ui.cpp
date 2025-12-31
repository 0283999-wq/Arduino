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
};

static UIStateCache uiCache;
static unsigned long lastHudRefresh = 0;
static unsigned long lastBatteryRefresh = 0;
static unsigned long lastBackgroundRefresh = 0;
static unsigned long lastPulse = 0;
static unsigned long lastVolumeOverlay = 0;
static bool backgroundDrawn = false;
static bool pulseWasActive = false;
static bool volumeOverlayActive = false;
static float overlayVolumeLerp = DEFAULT_VOLUME;

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
  // Circular safe area outline
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS, COLOR_DARK);
  display.drawCircle(CENTER_X, CENTER_Y, UI_SAFE_RADIUS - 2, COLOR_PANEL);

  // Subtle grid clipped to the circle
  for (int16_t x = UI_SAFE_LEFT + 8; x < UI_SAFE_LEFT + UI_SAFE_DIAMETER; x += 14) {
    drawSafeGridLineV(x);
  }
  for (int16_t y = UI_SAFE_TOP + 10; y < UI_SAFE_TOP + UI_SAFE_DIAMETER; y += 14) {
    drawSafeGridLineH(y);
  }

  // Concentric accents
  for (uint16_t r = 24; r < UI_SAFE_RADIUS; r += 22) {
    display.drawCircle(CENTER_X, CENTER_Y, r, COLOR_DARK);
  }

  // Minimal scanline overlay
  for (uint16_t y = UI_SAFE_TOP; y < UI_SAFE_TOP + UI_SAFE_DIAMETER; y += UI_SCANLINE_SPACING) {
    drawSafeGridLineH(y);
  }
}

static void drawPanel(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill) {
  display.fillRoundRect(x, y, w, h, 6, fill);
  display.drawRoundRect(x, y, w, h, 6, border);
}

static void drawWarningBar(const BatteryStatus &bat) {
  const int16_t x = UI_SAFE_LEFT + 10;
  const int16_t y = UI_SAFE_TOP + 12;
  const int16_t w = UI_SAFE_DIAMETER - 20;
  const int16_t h = 18;
  bool warn = (bat.percent < UI_WARNING_THRESHOLD || bat.level == BatteryLevel::Red);
  uint16_t fill = warn ? COLOR_WARNING : COLOR_PANEL;
  uint16_t border = warn ? COLOR_AMBER : COLOR_GRID;
  drawPanel(x, y, w, h, border, fill);

  display.setTextSize(1);
  display.setTextColor(warn ? COLOR_BG : COLOR_TEXT, fill);
  display.setCursor(x + 8, y + 4);
  if (warn) {
    display.print("WARNING! LOW POWER");
  } else {
    display.print("SPECTRA // SYSTEM ONLINE");
  }
}

static void drawTrackPanel(const AudioStatus &audio, bool accent) {
  const int16_t x = UI_SAFE_LEFT + 12;
  const int16_t y = UI_SAFE_TOP + 36;
  const int16_t w = UI_SAFE_DIAMETER - 24;
  const int16_t h = 42;
  uint16_t border = accent ? COLOR_AMBER : COLOR_ACCENT;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(2);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 10);
  display.print("TRACK ");
  char buf[6];
  sprintf(buf, "%04d", audio.track);
  display.print(buf);

  display.setTextSize(1);
  display.setCursor(x + w - 64, y + 12);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.print("OF ");
  sprintf(buf, "%03d", audio.trackCount);
  display.print(buf);
}

static void drawStatePanel(const AudioStatus &audio, bool accent) {
  const int16_t x = UI_SAFE_LEFT + 12;
  const int16_t y = UI_SAFE_TOP + 82;
  const int16_t w = UI_SAFE_DIAMETER - 24;
  const int16_t h = 32;
  uint16_t border = accent ? COLOR_AMBER : COLOR_GRID;
  drawPanel(x, y, w, h, border, COLOR_PANEL);

  display.setTextSize(2);
  display.setCursor(x + 10, y + 8);
  if (!audio.online) {
    display.setTextColor(COLOR_AMBER, COLOR_PANEL);
    display.print("AUDIO OFFLINE");
  } else if (audio.state == PlaybackState::Playing) {
    display.setTextColor(COLOR_TEXT, COLOR_PANEL);
    display.print("PLAY");
  } else {
    display.setTextColor(COLOR_AMBER, COLOR_PANEL);
    display.print("PAUSE");
  }
}

static void drawMessagePanel() {
  const int16_t x = UI_SAFE_LEFT + 12;
  const int16_t y = UI_SAFE_TOP + 122;
  const int16_t w = UI_SAFE_DIAMETER - 24;
  const int16_t h = 26;
  drawPanel(x, y, w, h, COLOR_GRID, COLOR_PANEL);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_PANEL);
  display.setCursor(x + 10, y + 7);
  display.print("AUDIO CORE // FIELD UNIT");
}

static void drawVolumePanel(const AudioStatus &audio) {
  const int16_t x = UI_SAFE_LEFT + 12;
  const int16_t y = UI_SAFE_TOP + 156;
  const int16_t w = UI_SAFE_DIAMETER - 24;
  const int16_t h = 34;
  drawPanel(x, y, w, h, COLOR_GRID, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 6);
  display.print("VOLUME");

  uint16_t barX = x + 10;
  uint16_t barY = y + 18;
  uint16_t barW = w - 20;
  uint16_t barH = 10;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map(audio.volume, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void drawBatteryPanel(const BatteryStatus &bat) {
  const int16_t x = UI_SAFE_LEFT + 12;
  const int16_t y = UI_SAFE_TOP + 198;
  const int16_t w = UI_SAFE_DIAMETER - 24;
  const int16_t h = 28;
  drawPanel(x, y, w, h, COLOR_GRID, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 8);
  display.print("BAT ");
  display.print(bat.voltage, 2);
  display.print("V  ");
  display.print(bat.percent);
  display.print("%");

  uint16_t iconX = x + w - 60;
  uint16_t iconY = y + 6;
  uint16_t iconW = 42;
  uint16_t iconH = h - 12;
  display.drawRoundRect(iconX, iconY, iconW, iconH, 3, COLOR_GRID);
  display.fillRect(iconX + iconW, iconY + 3, 4, iconH - 6, COLOR_GRID);

  uint16_t fill = map(bat.percent, 0, 100, 0, iconW - 4);
  uint16_t color = COLOR_AMBER;
  if (bat.level == BatteryLevel::Green) color = COLOR_TEXT;
  if (bat.level == BatteryLevel::Red) color = COLOR_WARNING;
  display.fillRoundRect(iconX + 2, iconY + 2, fill, iconH - 4, 2, color);
}

static void drawVolumeOverlayBar(uint8_t volume, float lerpValue) {
  const int16_t w = UI_SAFE_DIAMETER - 40;
  const int16_t h = 30;
  const int16_t x = UI_SAFE_LEFT + 20;
  const int16_t y = CENTER_Y - h / 2;

  display.fillRoundRect(x, y, w, h, 8, COLOR_BG);
  display.drawRoundRect(x, y, w, h, 8, COLOR_ACCENT);
  display.setTextSize(1);
  display.setTextColor(COLOR_ACCENT, COLOR_BG);
  display.setCursor(x + 10, y + 10);
  display.print("VOL");

  uint16_t barX = x + 42;
  uint16_t barY = y + 10;
  uint16_t barW = w - 54;
  uint16_t barH = 10;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_ACCENT);
  uint16_t fill = map((uint16_t)lerpValue, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 2, COLOR_TEXT);
}

static void updateVolumeOverlay(const AudioStatus &audio, unsigned long now) {
  if (volumeOverlayActive) {
    float target = audio.volume;
    overlayVolumeLerp = overlayVolumeLerp + (target - overlayVolumeLerp) * 0.35f;
    drawVolumeOverlayBar(audio.volume, overlayVolumeLerp);
    if (now - lastVolumeOverlay > UI_VOLUME_OVERLAY_MS) {
      volumeOverlayActive = false;
      // Clear overlay area softly
      const int16_t w = UI_SAFE_DIAMETER - 40;
      const int16_t h = 30;
      const int16_t x = UI_SAFE_LEFT + 20;
      const int16_t y = CENTER_Y - h / 2;
      display.fillRoundRect(x, y, w, h, 8, COLOR_BG);
    }
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

void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery) {
  unsigned long now = millis();

  if (!backgroundDrawn || now - lastBackgroundRefresh > UI_BACKGROUND_REFRESH_MS) {
    drawBackground();
    backgroundDrawn = true;
    lastBackgroundRefresh = now;
    uiCache.initialized = false;
  }

  bool pulseActive = (now - lastPulse) <= UI_ANIM_MS;
  bool audioDiff = !uiCache.initialized || audioChanged(audio, uiCache.audio) || pulseActive;
  bool batDiff = !uiCache.initialized || batteryChanged(battery, uiCache.battery);

  if ((audioDiff || !uiCache.initialized) && (now - lastHudRefresh >= UI_HUD_REFRESH_MS)) {
    drawTrackPanel(audio, pulseActive);
    drawStatePanel(audio, pulseActive);
    drawMessagePanel();
    drawVolumePanel(audio);
    lastHudRefresh = now;
  }

  if (batDiff || (now - lastBatteryRefresh >= UI_BATTERY_REFRESH_MS)) {
    drawBatteryPanel(battery);
    drawWarningBar(battery);
    lastBatteryRefresh = now;
  }

  updateVolumeOverlay(audio, now);

  uiCache.audio = audio;
  uiCache.battery = battery;
  uiCache.initialized = true;
  pulseWasActive = pulseActive;
}

void uiPulse(const char *label) {
  (void)label; // pulse label unused in the streamlined HUD
  lastPulse = millis();
}

void uiShowVolumeOverlay() {
  lastVolumeOverlay = millis();
  volumeOverlayActive = true;
  overlayVolumeLerp = uiCache.initialized ? uiCache.audio.volume : DEFAULT_VOLUME;
}
