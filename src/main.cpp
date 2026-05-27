#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "sprites/title/title.h"
#include "sprites/credits/credits.h"
#include "sprites/tia/tia.h"
#include "sprites/speech/speech.h"
#include "buttons.h"


#define SCREEN_SDA_PIN 22
#define SCREEN_SCL_PIN 23

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCREEN_SCL_PIN, SCREEN_SDA_PIN, U8X8_PIN_NONE);

enum class GameState { TITLE, CREDIT, EGG, HATCHING, ALIVE, DIED };

GameState gameState = GameState::TITLE;
unsigned long creditEnteredAt = 0;
unsigned long hatchStartedAt  = 0;

void drawTitle() {
  u8g2.drawXBMP(0, 0, 128, 30, title_f0);
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr(20, 48, "Press any button");
}

void drawEgg() {
  // Wobble pattern: mostly centred, occasional left/right nudges
  static const int8_t shakeX[] = { 0, -2, 0, 2, 0, -1, 0, 1 };
  int8_t ox = shakeX[(millis() / 120) % 8];
  uint8_t frame = (millis() / 500) % EGG_FRAMES;
  u8g2.drawXBMP(48 + ox, 16, EGG_WIDTH, EGG_HEIGHT, egg_frames[frame]);
}

void drawHatching() {
  // Step through Tia frames 0→1→2, 2s each; clamp at frame 2
  unsigned long elapsed = millis() - hatchStartedAt;
  uint8_t frame = (uint8_t)(elapsed / 2000);
  if (frame > 2) frame = 2;
  u8g2.drawXBMP(16, 16, TIA_WIDTH, TIA_HEIGHT, tia_frames[frame]);
}

void drawAlive() {
  // Tia frame 2 on the left, speech bubble to her right
  u8g2.drawXBMP(16, 16, TIA_WIDTH, TIA_HEIGHT, tia_frames[2]);
  u8g2.drawXBMP(16 + TIA_WIDTH + 4, 8, SPEECH_WIDTH, SPEECH_HEIGHT, speech_f0);
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
    case GameState::EGG:
      if (anyButton) {
        Serial.println("time for egg to hatch");
        gameState = GameState::HATCHING;
        hatchStartedAt = millis();
      }
      break;
    case GameState::HATCHING:
      if (millis() - hatchStartedAt >= 6000) gameState = GameState::ALIVE;
      break;
    case GameState::ALIVE: break;
    case GameState::DIED:  break;
  }

  // --- Draw ---
  u8g2.clearBuffer();

  switch (gameState) {
    case GameState::TITLE:  drawTitle();   break;
    case GameState::CREDIT: drawCredits(); break;
    case GameState::EGG:      drawEgg();      break;
    case GameState::HATCHING: drawHatching(); break;
    case GameState::ALIVE:    drawAlive();    break;
    case GameState::DIED:  break;
  }

  u8g2.sendBuffer();
}
