#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "sprites/title/title.h"
#include "sprites/credits/credits.h"
#include "sprites/tia/tia.h"
#include "sprites/speech/speech.h"
#include "sprites/cross/cross.h"
#include "buttons.h"


#define SCREEN_SDA_PIN 22
#define SCREEN_SCL_PIN 23

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCREEN_SCL_PIN, SCREEN_SDA_PIN, U8X8_PIN_NONE);

enum class GameState { TITLE, CREDIT, EGG, HATCHING, ALIVE, DIED };

static const bool DEV_MODE = true;

GameState gameState = DEV_MODE ? GameState::ALIVE : GameState::TITLE;
unsigned long creditEnteredAt = 0;
unsigned long hatchStartedAt  = 0;
unsigned long aliveStartedAt  = 0;

// Depletion rate in bar units (0–100) per minute for each stat
static const uint8_t HUNGER_DEPLETION_PER_MIN = 5;
static const uint8_t LOVE_DEPLETION_PER_MIN   = 5;

// Alert thresholds — serial warning fires once when level crosses below
static const uint8_t HUNGER_ALERT_THRESHOLD = 20;
static const uint8_t LOVE_ALERT_THRESHOLD   = 20;

static const uint8_t HUNGER_DEFAULT = 100;
static const uint8_t LOVE_DEFAULT   = 100;

uint8_t hungerLevel = HUNGER_DEFAULT;
uint8_t loveLevel   = LOVE_DEFAULT;

bool hungerAlertFired = false;
bool loveAlertFired   = false;

unsigned long lastHungerDepletedAt = 0;
unsigned long lastLoveDepletedAt   = 0;

void resetGame() {
  hungerLevel      = HUNGER_DEFAULT;
  loveLevel        = LOVE_DEFAULT;
  hungerAlertFired = false;
  loveAlertFired   = false;
  gameState        = GameState::TITLE;
}

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

// 7×6 heart icon (XBM: LSB-first, one byte per row)
static const uint8_t heartIcon[] PROGMEM = { 0x36, 0x7F, 0x7F, 0x3E, 0x1C, 0x08 };

void drawStatBars() {
  u8g2.setFont(u8g2_font_4x6_tr);

  // Hunger bar — left half
  u8g2.drawStr(1, 7, "H");
  u8g2.drawFrame(8, 1, 54, 6);
  u8g2.drawBox(9, 2, (52 * hungerLevel) / 100, 4);

  // Love bar — right half (heart icon instead of "L")
  u8g2.drawXBMP(66, 1, 7, 6, heartIcon);
  u8g2.drawFrame(75, 1, 52, 6);
  u8g2.drawBox(76, 2, (50 * loveLevel) / 100, 4);
}

void drawAlive() {
  drawStatBars();

  bool showSpeech = (millis() - aliveStartedAt) < 2000;

  if (showSpeech) {
    // Tia left, speech bubble to her right
    u8g2.drawXBMP(16, 10, TIA_WIDTH, TIA_HEIGHT, tia_frames[2]);
    u8g2.drawXBMP(16 + TIA_WIDTH + 4, 10, SPEECH_WIDTH, SPEECH_HEIGHT, speech_f0);
  } else {
    // Tia centred in the space below the stat bars
    uint8_t tiaX = (128 - TIA_WIDTH) / 2;
    uint8_t tiaY = 8 + (56 - TIA_HEIGHT) / 2;
    u8g2.drawXBMP(tiaX, tiaY, TIA_WIDTH, TIA_HEIGHT, tia_frames[2]);
  }
}

void drawDied() {
  u8g2.setFont(u8g2_font_9x15_tr);
  u8g2.drawStr((128 - 9 * 8) / 2, 12, "Tia Died");
  u8g2.drawXBMP((128 - CROSS_WIDTH) / 2, 18, CROSS_WIDTH, CROSS_HEIGHT, cross_f0);
}

void drawCredits() {
  u8g2.drawXBMP(0, 0, 128, 64, credits_f0);
}

void setup() {
  Serial.begin(115200);
  buttonsSetup();
  u8g2.begin();
  if (DEV_MODE) {
    aliveStartedAt       = millis();
    lastHungerDepletedAt = aliveStartedAt;
    lastLoveDepletedAt   = aliveStartedAt;
  }
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
      if (millis() - hatchStartedAt >= 6000) {
        gameState = GameState::ALIVE;
        aliveStartedAt = millis();
        lastHungerDepletedAt = aliveStartedAt;
        lastLoveDepletedAt   = aliveStartedAt;
      }
      break;
    case GameState::ALIVE: {
      unsigned long now = millis();
      if (now - lastHungerDepletedAt >= (60000UL / HUNGER_DEPLETION_PER_MIN)) {
        if (hungerLevel > 0) hungerLevel--;
        lastHungerDepletedAt = now;
        if (!hungerAlertFired && hungerLevel < HUNGER_ALERT_THRESHOLD) {
          Serial.print("WARNING: hunger below "); Serial.print(HUNGER_ALERT_THRESHOLD); Serial.println("%");
          hungerAlertFired = true;
        }
      }
      if (now - lastLoveDepletedAt >= (60000UL / LOVE_DEPLETION_PER_MIN)) {
        if (loveLevel > 0) loveLevel--;
        lastLoveDepletedAt = now;
        if (!loveAlertFired && loveLevel < LOVE_ALERT_THRESHOLD) {
          Serial.print("WARNING: love below "); Serial.print(LOVE_ALERT_THRESHOLD); Serial.println("%");
          loveAlertFired = true;
        }
      }
      if (hungerLevel == 0 || loveLevel == 0) {
        Serial.println("Tia died.");
        gameState = GameState::DIED;
      }
      break;
    }
    case GameState::DIED:
      if (anyButton) resetGame();
      break;
  }

  // --- Draw ---
  u8g2.clearBuffer();

  switch (gameState) {
    case GameState::TITLE:  drawTitle();   break;
    case GameState::CREDIT: drawCredits(); break;
    case GameState::EGG:      drawEgg();      break;
    case GameState::HATCHING: drawHatching(); break;
    case GameState::ALIVE:    drawAlive();    break;
    case GameState::DIED: drawDied(); break;
  }

  u8g2.sendBuffer();
}
