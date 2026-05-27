#pragma once
#include <Arduino.h>

#define BTN_LEFT   14
#define BTN_MIDDLE 27
#define BTN_RIGHT  26

void buttonsSetup();

bool leftButtonPressed();
bool middleButtonPressed();
bool rightButtonPressed();
