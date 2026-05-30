#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "sprites/title/title.h"
#include "sprites/credits/credits.h"
#include "sprites/tia/tia.h"
#include "sprites/speech/speech.h"
#include "sprites/cross/cross.h"
#include "sprites/pizza/pizza.h"
#include "buttons.h"


#define SCREEN_SDA_PIN 22
#define SCREEN_SCL_PIN 23

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCREEN_SCL_PIN, SCREEN_SDA_PIN, U8X8_PIN_NONE);

enum class GameState { TITLE, CREDIT, EGG, HATCHING, ALIVE, SLEEPING, MENU, FEEDING, DIED };

static const bool DEV_MODE = true;

GameState gameState = DEV_MODE ? GameState::ALIVE : GameState::TITLE;
unsigned long creditEnteredAt = 0;
unsigned long hatchStartedAt  = 0;
unsigned long aliveStartedAt    = 0;
unsigned long sleepingStartedAt = 0;
unsigned long feedingStartedAt  = 0;

static const uint16_t FEEDING_DURATION_MS = 3000;

// Depletion rate in bar units (0–100) per minute for each stat
static const uint8_t HUNGER_DEPLETION_PER_MIN = 25;
static const uint8_t LOVE_DEPLETION_PER_MIN   = 25;
static const uint8_t EEP_ACCUMULATE_PER_MIN   = 25;  // rate eepiness builds while awake
static const uint8_t EEP_RECHARGE_PER_MIN     = 50; // rate eepiness drains while sleeping

// Alert thresholds
static const uint8_t HUNGER_ALERT_THRESHOLD = 20;
static const uint8_t LOVE_ALERT_THRESHOLD   = 20;
static const uint8_t EEP_ALERT_THRESHOLD    = 80; // fires when eepiness gets high

static const uint8_t HUNGER_DEFAULT = 100;
static const uint8_t LOVE_DEFAULT   = 100;
static const uint8_t EEP_DEFAULT    = 80;

uint8_t hungerLevel = HUNGER_DEFAULT;
uint8_t loveLevel   = LOVE_DEFAULT;
uint8_t eepLevel    = EEP_DEFAULT;

bool hungerAlertFired = false;
bool loveAlertFired   = false;
bool eepAlertFired    = false;

unsigned long lastHungerDepletedAt = 0;
unsigned long lastLoveDepletedAt   = 0;
unsigned long lastEepDepletedAt    = 0;

GameState menuReturnState  = GameState::ALIVE;
uint8_t   menuSelectedIndex = 0;

static const char* const MENU_OPTIONS_ALIVE[]    = { "Close menu", "Go to eep", "Feed the Tia" };
static const char* const MENU_OPTIONS_SLEEPING[] = { "Close menu", "Wake up" };
const char* const* activeMenuOptions = MENU_OPTIONS_ALIVE;
uint8_t activeMenuOptionCount = 3;

bool isInGameLoop() {
  return gameState == GameState::ALIVE
      || gameState == GameState::SLEEPING
      || gameState == GameState::FEEDING;
}

void resetGame() {
  hungerLevel      = HUNGER_DEFAULT;
  loveLevel        = LOVE_DEFAULT;
  eepLevel         = EEP_DEFAULT;
  hungerAlertFired = false;
  loveAlertFired   = false;
  eepAlertFired    = false;
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

  // Hunger — section 0..39 (40px), 4px gap, Sleep — 44..83, 4px gap, Love — 88..127
  u8g2.drawStr(1, 7, "H");
  u8g2.drawFrame(6, 1, 33, 6);
  u8g2.drawBox(7, 2, (31 * hungerLevel) / 100, 4);

  u8g2.drawStr(44, 7, "zZ");
  u8g2.drawFrame(53, 1, 30, 6);
  u8g2.drawBox(54, 2, (28 * eepLevel) / 100, 4);

  u8g2.drawXBMP(88, 1, 7, 6, heartIcon);
  u8g2.drawFrame(96, 1, 31, 6);
  u8g2.drawBox(97, 2, (29 * loveLevel) / 100, 4);
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

void drawSleeping() {
  drawStatBars();
  uint8_t frame = 3 + (millis() / 500) % 2; // loops tia_f3 ↔ tia_f4
  uint8_t tiaX = (128 - TIA_WIDTH) / 2;
  uint8_t tiaY = 8 + (56 - TIA_HEIGHT) / 2;
  u8g2.drawXBMP(tiaX, tiaY, TIA_WIDTH, TIA_HEIGHT, tia_frames[frame]);
}

void drawMenu() {
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((128 - 5 * 4) / 2, 8, "MENU");
  u8g2.drawHLine(0, 10, 128);
  for (uint8_t i = 0; i < activeMenuOptionCount; i++) {
    uint8_t y = 22 + i * 14;
    if (i == menuSelectedIndex) {
      u8g2.drawBox(0, y - 8, 128, 10);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, y, activeMenuOptions[i]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, y, activeMenuOptions[i]);
    }
  }
}

void drawFeeding() {
  drawStatBars();
  uint8_t tiaFrame = 5 + (millis() / 100) % 2; // alternates tia_f5 ↔ tia_f6
  uint8_t tiaY   = 8 + (56 - TIA_HEIGHT)  / 2;
  uint8_t pizzaY = 8 + (56 - PIZZA_HEIGHT) / 2;
  u8g2.drawXBMP(0,  tiaY,   TIA_WIDTH,  TIA_HEIGHT,  tia_frames[tiaFrame]);
  u8g2.drawXBMP(TIA_WIDTH + 4, pizzaY, PIZZA_WIDTH, PIZZA_HEIGHT, pizza_f0);
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
    lastEepDepletedAt    = aliveStartedAt;
  }
}

void loop() {
  // --- Input ---
  bool leftBtn   = leftButtonPressed();
  bool middleBtn = middleButtonPressed();
  bool rightBtn  = rightButtonPressed();
  bool anyButton = leftBtn || middleBtn || rightBtn;

  // --- State transitions ---
  if (isInGameLoop()) {
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
  }

  if (isInGameLoop() && middleBtn) {
    menuReturnState   = gameState;
    menuSelectedIndex = 0;
    activeMenuOptions     = (gameState == GameState::SLEEPING) ? MENU_OPTIONS_SLEEPING : MENU_OPTIONS_ALIVE;
    activeMenuOptionCount = (gameState == GameState::SLEEPING) ? 2 : 3;
    gameState         = GameState::MENU;
    middleBtn         = false; // prevent MENU case acting on the same press
  }

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
        lastEepDepletedAt    = aliveStartedAt;
      }
      break;
    case GameState::ALIVE: {
      unsigned long now = millis();
      if (now - lastEepDepletedAt >= (60000UL / EEP_ACCUMULATE_PER_MIN)) {
        if (eepLevel < 100) eepLevel++;
        lastEepDepletedAt = now;
        if (!eepAlertFired && eepLevel >= EEP_ALERT_THRESHOLD) {
          Serial.print("WARNING: getting sleepy - eep at "); Serial.print(eepLevel); Serial.println("%");
          eepAlertFired = true;
        }
        if (eepLevel >= 100) {
          Serial.println("Tia fell asleep.");
          sleepingStartedAt = millis();
          lastEepDepletedAt = sleepingStartedAt;
          eepAlertFired     = false;
          gameState         = GameState::SLEEPING;
        }
      }
      break;
    }
    case GameState::SLEEPING: {
      unsigned long now = millis();
      if (now - lastEepDepletedAt >= (60000UL / EEP_RECHARGE_PER_MIN)) {
        if (eepLevel > 0) eepLevel--;
        lastEepDepletedAt = now;
      }
      if (eepLevel == 0) {
        Serial.println("Tia woke up.");
        aliveStartedAt       = millis();
        lastHungerDepletedAt = aliveStartedAt;
        lastLoveDepletedAt   = aliveStartedAt;
        lastEepDepletedAt    = aliveStartedAt;
        gameState            = GameState::ALIVE;
      }
      break;
    }
    case GameState::MENU: {
      if (leftBtn)  menuSelectedIndex = (menuSelectedIndex + activeMenuOptionCount - 1) % activeMenuOptionCount;
      if (rightBtn) menuSelectedIndex = (menuSelectedIndex + 1) % activeMenuOptionCount;
      if (middleBtn) {
        switch (menuSelectedIndex) {
          case 0: // Close menu
            gameState = menuReturnState;
            break;
          case 1: // Go to eep / Wake up
            if (menuReturnState == GameState::SLEEPING) {
              aliveStartedAt       = millis();
              lastHungerDepletedAt = aliveStartedAt;
              lastLoveDepletedAt   = aliveStartedAt;
              lastEepDepletedAt    = aliveStartedAt;
              gameState            = GameState::ALIVE;
            } else {
              sleepingStartedAt = millis();
              lastEepDepletedAt = sleepingStartedAt;
              gameState         = GameState::SLEEPING;
            }
            break;
          case 2: // Feed the Tia
            feedingStartedAt = millis();
            gameState        = GameState::FEEDING;
            break;
        }
      }
      break;
    }
    case GameState::FEEDING:
      if (millis() - feedingStartedAt >= FEEDING_DURATION_MS) {
        hungerLevel  = 100;
        aliveStartedAt = millis();
        lastHungerDepletedAt = aliveStartedAt;
        lastLoveDepletedAt   = aliveStartedAt;
        lastEepDepletedAt    = aliveStartedAt;
        gameState    = GameState::ALIVE;
      }
      break;
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
    case GameState::SLEEPING: drawSleeping(); break;
    case GameState::MENU:     drawMenu();     break;
    case GameState::FEEDING:  drawFeeding();  break;
    case GameState::DIED:     drawDied();     break;
  }

  u8g2.sendBuffer();
}
