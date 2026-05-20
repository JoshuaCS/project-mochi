#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

enum class GameState { EGG, ALIVE, DIED };

GameState gameState = GameState::EGG;

void drawEgg() {
  u8g2.drawXBMP(48, 16, EGG_WIDTH, EGG_HEIGHT, egg_f0);
}

void setup() {
  Serial.begin(115200);
  u8g2.begin();
}

void loop() {
  u8g2.clearBuffer();

  switch (gameState) {
    case GameState::EGG:   drawEgg(); break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  u8g2.sendBuffer();
}
