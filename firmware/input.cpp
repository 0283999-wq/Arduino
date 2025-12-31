#include "input.h"

struct ButtonState {
  uint8_t pin;
  bool activeHigh;
  bool lastStable;
  bool lastReading;
  unsigned long lastChange;
  unsigned long lastRepeat;
};

static ButtonState touchPlay{PIN_TOUCH_PLAY, true, false, false, 0, 0};
static ButtonState touchNext{PIN_TOUCH_NEXT, true, false, false, 0, 0};
static ButtonState touchPrev{PIN_TOUCH_PREV, true, false, false, 0, 0};
static ButtonState btnVolDown{PIN_BTN_VOL_DOWN, false, true, true, 0, 0};
static ButtonState btnVolUp{PIN_BTN_VOL_UP, false, true, true, 0, 0};

static void primeButton(ButtonState &btn) {
  bool reading = digitalRead(btn.pin);
  btn.lastReading = reading;
  btn.lastStable = btn.activeHigh ? reading : !reading;
  btn.lastChange = millis();
  btn.lastRepeat = btn.lastChange;
}

static bool readPressed(const ButtonState &btn) {
  bool raw = digitalRead(btn.pin);
  return btn.activeHigh ? raw : !raw;
}

static InputEvent processMomentary(ButtonState &btn, InputEvent eventForPress) {
  bool pressed = readPressed(btn);
  unsigned long now = millis();

  if (pressed != btn.lastReading) {
    btn.lastChange = now;
    btn.lastReading = pressed;
  }

  if ((now - btn.lastChange) > DEBOUNCE_MS) {
    if (pressed != btn.lastStable) {
      btn.lastStable = pressed;
      if (pressed) {
        return eventForPress;
      }
    }
  }
  return InputEvent::None;
}

static InputEvent processRepeat(ButtonState &btn, InputEvent eventForPress) {
  bool pressed = readPressed(btn);
  unsigned long now = millis();

  if (pressed != btn.lastReading) {
    btn.lastChange = now;
    btn.lastReading = pressed;
  }

  if ((now - btn.lastChange) > DEBOUNCE_MS) {
    if (pressed != btn.lastStable) {
      btn.lastStable = pressed;
      if (pressed) {
        btn.lastRepeat = now;
        return eventForPress;
      }
    } else if (pressed && (now - btn.lastRepeat) > REPEAT_MS) {
      btn.lastRepeat = now;
      return eventForPress;
    }
  }
  return InputEvent::None;
}

void inputInit() {
  pinMode(PIN_TOUCH_PLAY, INPUT);
  pinMode(PIN_TOUCH_NEXT, INPUT);
  pinMode(PIN_TOUCH_PREV, INPUT);

  pinMode(PIN_BTN_VOL_DOWN, INPUT_PULLUP);
  pinMode(PIN_BTN_VOL_UP, INPUT_PULLUP);

  primeButton(touchPlay);
  primeButton(touchNext);
  primeButton(touchPrev);
  primeButton(btnVolDown);
  primeButton(btnVolUp);
}

InputEvent inputPoll() {
  InputEvent e = processMomentary(touchPlay, InputEvent::PlayPause);
  if (e != InputEvent::None) return e;

  e = processMomentary(touchNext, InputEvent::Next);
  if (e != InputEvent::None) return e;

  e = processMomentary(touchPrev, InputEvent::Prev);
  if (e != InputEvent::None) return e;

  e = processRepeat(btnVolUp, InputEvent::VolUp);
  if (e != InputEvent::None) return e;

  e = processRepeat(btnVolDown, InputEvent::VolDown);
  if (e != InputEvent::None) return e;

  return InputEvent::None;
}

