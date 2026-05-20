#pragma once
#include <Arduino.h>

#define BTN_LEFT   12
#define BTN_MIDDLE 13
#define BTN_RIGHT  14

void buttonsSetup();
bool leftButtonPressed();
bool middleButtonPressed();
bool rightButtonPressed();
