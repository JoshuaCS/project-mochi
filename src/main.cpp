#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "buttons.h"

U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

enum class GameState { TITLE, EGG, ALIVE, DIED };

GameState gameState = GameState::TITLE;

void drawTitle() {
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.drawStr(28, 28, "TIAGOTCHI");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(16, 48, "Press any button");
}

void drawEgg() {
  u8g2.drawXBMP(48, 16, EGG_WIDTH, EGG_HEIGHT, egg_f0);
}

void setup() {
  Serial.begin(115200);
  buttonsSetup();
  u8g2.begin();
}

void loop() {
  // --- Input ---
  bool anyButton = leftButtonPressed() || middleButtonPressed() || rightButtonPressed();

  // --- State transitions ---
  switch (gameState) {
    case GameState::TITLE:
      if (anyButton) gameState = GameState::EGG;
      break;
    case GameState::EGG:   break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  // --- Draw ---
  u8g2.clearBuffer();

  switch (gameState) {
    case GameState::TITLE: drawTitle(); break;
    case GameState::EGG:   drawEgg();   break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  u8g2.sendBuffer();
}
