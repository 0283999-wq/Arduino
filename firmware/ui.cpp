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
static String pulseLabel;
static bool backgroundDrawn = false;
static bool pulseWasActive = false;

static void drawScanlines() {
  for (uint16_t y = 0; y < SCREEN_SIZE; y += UI_SCANLINE_SPACING) {
    display.drawFastHLine(0, y, SCREEN_SIZE, COLOR_DARK);
  }
}

static void drawDiagonalGrid() {
  for (int16_t x = -SCREEN_SIZE; x < SCREEN_SIZE * 2; x += 14) {
    display.drawLine(x, 0, x - SCREEN_SIZE, SCREEN_SIZE, COLOR_PANEL);
  }
}

static void drawConcentric() {
  for (uint16_t r = 24; r < CENTER_X; r += 18) {
    display.drawCircle(CENTER_X, CENTER_Y, r, COLOR_GRID);
  }
  for (uint16_t a = 0; a < 360; a += 20) {
    float rad = radians(a);
    uint16_t x = CENTER_X + cos(rad) * (CENTER_X - 6);
    uint16_t y = CENTER_Y + sin(rad) * (CENTER_Y - 6);
    display.drawLine(CENTER_X, CENTER_Y, x, y, COLOR_DARK);
  }
}

static void drawFrameRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t border, uint16_t fill) {
  display.fillRoundRect(x, y, w, h, 6, fill);
  display.drawRoundRect(x, y, w, h, 6, border);
}

static void drawBackground() {
  display.fillScreen(COLOR_BG);
  drawDiagonalGrid();
  drawConcentric();
  display.drawCircle(CENTER_X, CENTER_Y, CENTER_X - 4, COLOR_GRID);
  display.drawCircle(CENTER_X, CENTER_Y, CENTER_X - 10, COLOR_DARK);
  drawScanlines();
}

static void drawTrackPanel(const AudioStatus &audio) {
  const int16_t x = 18;
  const int16_t y = 24;
  const int16_t w = SCREEN_SIZE - 36;
  const int16_t h = 46;
  drawFrameRect(x, y, w, h, COLOR_GRID, COLOR_PANEL);

  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setTextSize(2);
  display.setCursor(x + 10, y + 12);
  display.print("TRACK ");
  char buf[6];
  sprintf(buf, "%04d", audio.track);
  display.print(buf);
}

static void drawStatePanel(const AudioStatus &audio, bool accent) {
  const int16_t x = 18;
  const int16_t y = 76;
  const int16_t w = SCREEN_SIZE - 36;
  const int16_t h = 34;
  drawFrameRect(x, y, w, h, accent ? COLOR_AMBER : COLOR_GRID, COLOR_PANEL);

  display.setTextSize(2);
  display.setCursor(x + 10, y + 8);
  if (audio.state == PlaybackState::Playing) {
    display.setTextColor(COLOR_TEXT, COLOR_PANEL);
    display.print("PLAY");
  } else {
    display.setTextColor(COLOR_AMBER, COLOR_PANEL);
    display.print("PAUSE");
  }
}

static void drawVolumePanel(const AudioStatus &audio) {
  const int16_t x = 18;
  const int16_t y = 118;
  const int16_t w = SCREEN_SIZE - 36;
  const int16_t h = 38;
  drawFrameRect(x, y, w, h, COLOR_GRID, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 6);
  display.print("AUDIO CORE ACTIVE");

  uint16_t barX = x + 10;
  uint16_t barY = y + 18;
  uint16_t barW = w - 20;
  uint16_t barH = 12;
  display.drawRoundRect(barX, barY, barW, barH, 3, COLOR_GRID);
  uint16_t fill = map(audio.volume, MIN_VOLUME, MAX_VOLUME, 0, barW - 4);
  display.fillRoundRect(barX + 2, barY + 2, fill, barH - 4, 3, COLOR_TEXT);
}

static void drawBatteryPanel(const BatteryStatus &bat) {
  const int16_t x = 18;
  const int16_t y = 164;
  const int16_t w = SCREEN_SIZE - 36;
  const int16_t h = 44;
  drawFrameRect(x, y, w, h, COLOR_GRID, COLOR_PANEL);

  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT, COLOR_PANEL);
  display.setCursor(x + 10, y + 6);
  display.print("BATT");

  uint16_t iconX = x + 10;
  uint16_t iconY = y + 18;
  uint16_t iconW = 26;
  uint16_t iconH = 12;
  display.drawRoundRect(iconX, iconY, iconW, iconH, 3, COLOR_GRID);
  display.fillRect(iconX + iconW, iconY + 3, 4, iconH - 6, COLOR_GRID);

  uint16_t fill = map(bat.percent, 0, 100, 0, iconW - 4);
  uint16_t color = COLOR_AMBER;
  if (bat.level == BatteryLevel::Green) color = COLOR_TEXT;
  if (bat.level == BatteryLevel::Red) color = COLOR_WARNING;
  display.fillRoundRect(iconX + 2, iconY + 2, fill, iconH - 4, 2, color);

  display.setCursor(x + 44, y + 18);
  display.setTextSize(2);
  display.setTextColor(color, COLOR_PANEL);
  display.print(bat.voltage, 2);
  display.print("V ");
  display.print(bat.percent);
  display.print("%");
}

static void drawPulseFooter(const char *label) {
  const int16_t x = 18;
  const int16_t y = 214;
  const int16_t w = SCREEN_SIZE - 36;
  const int16_t h = 18;
  display.fillRoundRect(x, y, w, h, 4, COLOR_BG);
  if (!label) return;
  display.drawRoundRect(x, y, w, h, 4, COLOR_AMBER);
  display.setTextSize(1);
  display.setTextColor(COLOR_AMBER, COLOR_BG);
  display.setCursor(x + 10, y + 5);
  display.print(label);
}

static void drawWarningPanel(const BatteryStatus &bat) {
  const int16_t x = 14;
  const int16_t y = 6;
  const int16_t w = SCREEN_SIZE - 28;
  const int16_t h = 16;
  if (bat.percent < UI_WARNING_THRESHOLD || bat.level == BatteryLevel::Red) {
    display.fillRoundRect(x, y, w, h, 4, COLOR_WARNING);
    display.drawRoundRect(x, y, w, h, 4, COLOR_AMBER);
    display.setTextSize(1);
    display.setTextColor(COLOR_BG, COLOR_WARNING);
    display.setCursor(x + 8, y + 4);
    display.print("WARNING! LOW POWER");
  } else {
    display.fillRoundRect(x, y, w, h, 4, COLOR_BG);
    display.drawRoundRect(x, y, w, h, 4, COLOR_PANEL);
    display.setTextColor(COLOR_TEXT, COLOR_BG);
    display.setCursor(x + 8, y + 4);
    display.print("SPECTRA SYSTEM ONLINE");
  }
}

void uiInit() {
  display.begin();
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

  bool pulseActive = (!pulseLabel.isEmpty() && (now - lastPulse <= UI_ANIM_MS));
  bool pulseEnded = (!pulseActive && pulseWasActive);
  pulseWasActive = pulseActive;

  bool audioChanged = !uiCache.initialized || audio.track != uiCache.audio.track || audio.volume != uiCache.audio.volume || audio.state != uiCache.audio.state || pulseEnded;
  bool batteryChanged = !uiCache.initialized || battery.percent != uiCache.battery.percent || battery.level != uiCache.battery.level || fabs(battery.voltage - uiCache.battery.voltage) > 0.02f;

  if ((audioChanged || pulseActive || pulseEnded || !uiCache.initialized) && (now - lastHudRefresh >= UI_HUD_REFRESH_MS)) {
    drawTrackPanel(audio);
    drawStatePanel(audio, pulseActive);
    drawVolumePanel(audio);
    drawPulseFooter(pulseActive ? pulseLabel.c_str() : nullptr);
    lastHudRefresh = now;
  }

  if (batteryChanged || (now - lastBatteryRefresh >= UI_BATTERY_REFRESH_MS)) {
    drawBatteryPanel(battery);
    drawWarningPanel(battery);
    lastBatteryRefresh = now;
  }

  uiCache.audio = audio;
  uiCache.battery = battery;
  uiCache.initialized = true;
}

void uiPulse(const char *label) {
  pulseLabel = label;
  lastPulse = millis();
}

