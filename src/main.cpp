#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "sprites/title/title.h"  
#include "sprites/credits/credits.h"
#include "buttons.h"


#define SCREEN_SDA_PIN 22
#define SCREEN_SCL_PIN 23

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCREEN_SCL_PIN, SCREEN_SDA_PIN, U8X8_PIN_NONE);

enum class GameState { TITLE, CREDIT, EGG, ALIVE, DIED };

GameState gameState = GameState::TITLE;
unsigned long creditEnteredAt = 0;

void drawTitle() {
  u8g2.drawXBMP(0, 0, 128, 30, title_f0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(20, 48, "Press any button");
}

void drawEgg() {
  u8g2.drawXBMP(48, 16, EGG_WIDTH, EGG_HEIGHT, egg_f0);
}

void drawCredits() {
  u8g2.drawXBMP(0, 0, 128, 64, credits_f0);
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
      if (anyButton) {
        gameState = GameState::CREDIT;
        creditEnteredAt = millis();
      }
      break;
    case GameState::CREDIT:
      if (millis() - creditEnteredAt >= 5000) gameState = GameState::EGG;
      break;
    case GameState::EGG:   break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  // --- Draw ---
  u8g2.clearBuffer();

  switch (gameState) {
    case GameState::TITLE:  drawTitle();   break;
    case GameState::CREDIT: drawCredits(); break;
    case GameState::EGG:    drawEgg();     break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  u8g2.sendBuffer();
}
