#include "ui.h"

#include <Adafruit_GFX.h>
#include <Adafruit_GC9A01A.h>
#include <SPI.h>

static Adafruit_GC9A01A display = Adafruit_GC9A01A(PIN_SCREEN_CS, PIN_SCREEN_DC, PIN_SCREEN_MOSI, PIN_SCREEN_SCK, PIN_SCREEN_RST);

static unsigned long lastRefresh = 0;
static unsigned long lastPulse = 0;
static String pulseLabel;

static void drawScanlines() {
  for (uint16_t y = 0; y < SCREEN_SIZE; y += UI_SCANLINE_SPACING) {
    display.drawFastHLine(0, y, SCREEN_SIZE, COLOR_DARK);
  }
}

static void drawGrid() {
  for (uint16_t r = 20; r < CENTER_X; r += 20) {
    display.drawCircle(CENTER_X, CENTER_Y, r, COLOR_GRID);
  }
  for (uint16_t a = 0; a < 360; a += 30) {
    float rad = radians(a);
    uint16_t x = CENTER_X + cos(rad) * (CENTER_X - 4);
    uint16_t y = CENTER_Y + sin(rad) * (CENTER_Y - 4);
    display.drawLine(CENTER_X, CENTER_Y, x, y, COLOR_GRID);
  }
}

static void drawBattery(const BatteryStatus &bat) {
  uint16_t barX = 30;
  uint16_t barY = SCREEN_SIZE - 40;
  uint16_t barW = SCREEN_SIZE - 60;
  uint16_t barH = 14;

  uint16_t color = COLOR_AMBER;
  if (bat.level == BatteryLevel::Green) color = COLOR_TEXT;
  if (bat.level == BatteryLevel::Red) color = COLOR_AMBER;

  display.drawRect(barX, barY, barW, barH, COLOR_GRID);
  uint16_t fill = map(bat.percent, 0, 100, 0, barW - 2);
  display.fillRect(barX + 1, barY + 1, fill, barH - 2, color);

  display.setCursor(barX, barY - 10);
  display.setTextColor(COLOR_TEXT);
  display.setTextSize(1);
  display.print("BATT ");
  display.print(bat.voltage, 2);
  display.print("V ");
  display.print(bat.percent);
  display.print("%");
}

static void drawTrackInfo(const AudioStatus &audio) {
  display.setTextColor(COLOR_TEXT);
  display.setTextSize(2);
  display.setCursor(40, 40);
  display.print("SPECTRA");

  display.setTextSize(1);
  display.setCursor(40, 60);
  display.print("SYSTEM ONLINE");

  display.setTextSize(2);
  display.setCursor(40, 90);
  display.print("TRACK ");
  char buf[6];
  sprintf(buf, "%04d", audio.track);
  display.print(buf);

  display.setTextSize(2);
  display.setCursor(40, 120);
  display.print(audio.state == PlaybackState::Playing ? "PLAY" : "PAUSE");
}

static void drawVolume(const AudioStatus &audio) {
  uint16_t x = 40;
  uint16_t y = 150;
  uint16_t w = SCREEN_SIZE - 80;
  uint16_t h = 14;

  display.drawRect(x, y, w, h, COLOR_GRID);
  uint16_t fill = map(audio.volume, MIN_VOLUME, MAX_VOLUME, 0, w - 2);
  display.fillRect(x + 1, y + 1, fill, h - 2, COLOR_TEXT);

  display.setCursor(x, y - 12);
  display.setTextSize(1);
  display.setTextColor(COLOR_TEXT);
  display.print("AUDIO CORE ACTIVE");
}

static void drawPulse() {
  if (pulseLabel.isEmpty()) return;
  unsigned long now = millis();
  if (now - lastPulse > UI_ANIM_MS) {
    pulseLabel = "";
    return;
  }
  display.setTextSize(2);
  display.setTextColor(COLOR_AMBER);
  display.setCursor(40, 190);
  display.print(pulseLabel);
  display.drawCircle(CENTER_X, CENTER_Y, 110, COLOR_AMBER);
}

void uiInit() {
  display.begin();
  display.fillScreen(COLOR_BG);
  drawGrid();
  drawScanlines();
}

void uiUpdate(const AudioStatus &audio, const BatteryStatus &battery) {
  unsigned long now = millis();
  if (now - lastRefresh < 100) return; // limit refresh rate
  lastRefresh = now;

  display.fillScreen(COLOR_BG);
  drawGrid();
  drawScanlines();
  drawTrackInfo(audio);
  drawVolume(audio);
  drawBattery(battery);
  drawPulse();
}

void uiPulse(const char *label) {
  pulseLabel = label;
  lastPulse = millis();
}

