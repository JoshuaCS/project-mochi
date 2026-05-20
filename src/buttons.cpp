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

bool leftButtonPressed()   { return checkPressed(BTN_LEFT,   left);   }
bool middleButtonPressed()  { return checkPressed(BTN_MIDDLE, middle);  }
bool rightButtonPressed()  { return checkPressed(BTN_RIGHT,  right);  }
