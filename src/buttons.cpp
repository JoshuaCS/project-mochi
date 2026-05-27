#include "buttons.h"

#define DEBOUNCE_MS 50

struct ButtonState {
  bool raw;
  bool confirmed;
  unsigned long debounceStart;
};

static ButtonState left   = { false, false, 0 };
static ButtonState middle = { false, false, 0 };
static ButtonState right  = { false, false, 0 };

// Serial key flags — set by pollSerial(), consumed once per read.
static bool serialLeft   = false;
static bool serialMiddle = false;
static bool serialRight  = false;

// Drain the serial buffer and latch any l/m/r keypresses.
static void pollSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if      (c == 'l' || c == 'L') serialLeft   = true;
    else if (c == 'm' || c == 'M') serialMiddle = true;
    else if (c == 'r' || c == 'R') serialRight  = true;
  }
}

void buttonsSetup() {
  pinMode(BTN_LEFT,   INPUT_PULLUP);
  pinMode(BTN_MIDDLE, INPUT_PULLUP);
  pinMode(BTN_RIGHT,  INPUT_PULLUP);
}

// Returns true once per press, after debounce settles.
static bool checkPressed(uint8_t pin, ButtonState& btn) {
  bool reading = digitalRead(pin) == LOW;
  unsigned long now = millis();

  if (reading != btn.raw) {
    btn.raw = reading;
    btn.debounceStart = now;
  }

  if ((now - btn.debounceStart) >= DEBOUNCE_MS && reading != btn.confirmed) {
    btn.confirmed = reading;
    return btn.confirmed;
  }
  return false;
}

bool leftButtonPressed() {
  pollSerial();
  if (serialLeft)   { serialLeft   = false; return true; }
  return checkPressed(BTN_LEFT, left);
}

bool middleButtonPressed() {
  pollSerial();
  if (serialMiddle) { serialMiddle = false; return true; }
  return checkPressed(BTN_MIDDLE, middle);
}

bool rightButtonPressed() {
  pollSerial();
  if (serialRight)  { serialRight  = false; return true; }
  return checkPressed(BTN_RIGHT, right);
}
