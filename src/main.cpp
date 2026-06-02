#include <Arduino.h>
#include <U8g2lib.h>
#include "sprites/egg/egg.h"
#include "sprites/title/title.h"
#include "sprites/credits/credits.h"
#include "sprites/tia/tia.h"
#include "sprites/kitties/kitties.h"
#include "sprites/speech/speech.h"
#include "sprites/cross/cross.h"
#include "sprites/happiness/happiness.h"
#include "sprites/pizza/pizza.h"
#include "buttons.h"


#define SCREEN_SDA_PIN 22
#define SCREEN_SCL_PIN 23

U8G2_SH1106_128X64_NONAME_F_SW_I2C u8g2(U8G2_R0, SCREEN_SCL_PIN, SCREEN_SDA_PIN, U8X8_PIN_NONE);

enum class GameState { TITLE, CREDIT, EGG, HATCHING, ALIVE, SLEEPING, MENU, FEEDING, INTERACTING, DIED };

static const bool DEV_MODE = false;

GameState gameState = DEV_MODE ? GameState::ALIVE : GameState::TITLE;
unsigned long creditEnteredAt = 0;
unsigned long hatchStartedAt  = 0;
unsigned long aliveStartedAt    = 0;
unsigned long sleepingStartedAt = 0;
unsigned long feedingStartedAt      = 0;
unsigned long interactingStartedAt  = 0;
uint8_t       interactingBonus      = 0;
const char*   interactingLabel      = "";

static const uint16_t FEEDING_DURATION_MS     = 3000;
static const uint16_t INTERACTION_DURATION_MS = 5000;

static const uint8_t HAPPINESS_PAT_BONUS     = 20;
static const uint8_t HAPPINESS_PLAY_BONUS    = 20;
static const uint8_t HAPPINESS_KITTIES_BONUS = 20;
static const uint8_t FEED_HUNGER_BONUS       = 50;

// Depletion rate in bar units (0–100) per minute for each stat
static const uint8_t HUNGER_DEPLETION_PER_MIN = 25;
static const uint8_t HAPPINESS_PASSIVE_DRAIN_PER_MIN = 20;  // always drains
static const uint8_t HAPPINESS_SLEEPY_DRAIN_PER_MIN  = 10; // extra drain when eep above threshold
static const uint8_t HAPPINESS_SLEEPY_THRESHOLD      = 70; // eep % that triggers extra drain
static const uint8_t HAPPINESS_FEED_BONUS            = 10; // happiness gained from feeding
static const uint8_t HAPPINESS_SLEEP_BONUS           = 10; // happiness gained from waking up
static const uint8_t EEP_ACCUMULATE_PER_MIN   = 25;  // rate eepiness builds while awake
static const uint8_t EEP_RECHARGE_PER_MIN     = 50; // rate eepiness drains while sleeping

// Alert thresholds
static const uint8_t HUNGER_ALERT_THRESHOLD = 20;
static const uint8_t HAPPINESS_ALERT_THRESHOLD   = 20;
static const uint8_t EEP_ALERT_THRESHOLD    = 80; // fires when eepiness gets high

static const uint8_t HUNGER_DEFAULT = 100;
static const uint8_t HAPPINESS_DEFAULT   = 100;
static const uint8_t EEP_DEFAULT    = 80;

uint8_t hungerLevel = HUNGER_DEFAULT;
uint8_t happinessLevel   = HAPPINESS_DEFAULT;
uint8_t eepLevel    = EEP_DEFAULT;

bool hungerAlertFired = false;
bool happinessAlertFired   = false;
bool eepAlertFired    = false;

unsigned long lastHungerDepletedAt        = 0;
unsigned long lastHappinessDepletedAt     = 0;
unsigned long lastHappinessSleepyAt       = 0;
unsigned long lastEepDepletedAt           = 0;

const char* deathReason = "";

GameState menuReturnState   = GameState::ALIVE;
uint8_t   menuSelectedIndex = 0;
uint8_t   menuScrollOffset  = 0;

static const char* const MENU_OPTIONS_ALIVE[]    = { "Close menu", "Go to eep", "Feed the Tia", "Pat the Tia", "Play with Kitties" };
static const char* const MENU_OPTIONS_SLEEPING[] = { "Close menu", "Wake up" };
const char* const* activeMenuOptions  = MENU_OPTIONS_ALIVE;
uint8_t            activeMenuOptionCount = 6;

bool isInGameLoop() {
  return gameState == GameState::ALIVE
      || gameState == GameState::SLEEPING
      || gameState == GameState::FEEDING
      || gameState == GameState::INTERACTING;
}

void addHunger(int16_t amount) {
  int16_t v = (int16_t)hungerLevel + amount;
  hungerLevel = (v < 0) ? 0 : (v > 100) ? 100 : (uint8_t)v;
}

void addHappiness(int16_t amount) {
  int16_t v = (int16_t)happinessLevel + amount;
  happinessLevel = (v < 0) ? 0 : (v > 100) ? 100 : (uint8_t)v;
}

void resetGame() {
  hungerLevel      = HUNGER_DEFAULT;
  happinessLevel        = HAPPINESS_DEFAULT;
  eepLevel         = EEP_DEFAULT;
  hungerAlertFired = false;
  happinessAlertFired   = false;
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


void drawStatBars() {
  u8g2.setFont(u8g2_font_4x6_tr);

  // Hunger — section 0..39 (40px), 4px gap, Sleep — 44..83, 4px gap, Love — 88..127
  u8g2.drawStr(1, 7, "H");
  u8g2.drawFrame(6, 1, 33, 6);
  u8g2.drawBox(7, 2, (31 * hungerLevel) / 100, 4);

  u8g2.drawStr(44, 7, "zZ");
  u8g2.drawFrame(53, 1, 30, 6);
  u8g2.drawBox(54, 2, (28 * eepLevel) / 100, 4);

  uint8_t happinessFrame = (happinessLevel >= 60) ? 0 : (happinessLevel >= 30) ? 1 : 2;
  u8g2.drawXBMP(88, 0, HAPPINESS_WIDTH, HAPPINESS_HEIGHT, happiness_frames[happinessFrame]);
  u8g2.drawFrame(96, 1, 31, 6);
  u8g2.drawBox(97, 2, (29 * happinessLevel) / 100, 4);
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
  for (uint8_t i = 0; i < 4 && menuScrollOffset + i < activeMenuOptionCount; i++) {
    uint8_t itemIdx = menuScrollOffset + i;
    uint8_t y = 20 + i * 12;
    if (itemIdx == menuSelectedIndex) {
      u8g2.drawBox(0, y - 7, 128, 9);
      u8g2.setDrawColor(0);
      u8g2.drawStr(4, y, activeMenuOptions[itemIdx]);
      u8g2.setDrawColor(1);
    } else {
      u8g2.drawStr(4, y, activeMenuOptions[itemIdx]);
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

void drawInteracting() {
  if (interactingLabel == "Play with Kitties") {
    uint8_t frame = (uint8_t)((millis() - interactingStartedAt) / 750) % KITTIES_FRAMES;
    u8g2.drawXBMP(0, 0, KITTIES_WIDTH, KITTIES_HEIGHT, kitties_frames[frame]);
  } else if (interactingLabel == "Pat the Tia") {
    drawStatBars();
    static const uint8_t patSeq[] = {7, 8, 9, 8};
    uint8_t seqIdx = (uint8_t)((millis() - interactingStartedAt) / 500) % 4;
    uint8_t tiaX = (128 - TIA_WIDTH) / 2;
    uint8_t tiaY = 8 + (56 - TIA_HEIGHT) / 2;
    u8g2.drawXBMP(tiaX, tiaY, TIA_WIDTH, TIA_HEIGHT, tia_frames[patSeq[seqIdx]]);
  } else {
    drawStatBars();
    u8g2.setFont(u8g2_font_5x7_tr);
    u8g2.drawStr((128 - u8g2.getStrWidth(interactingLabel)) / 2, 36, interactingLabel);
  }
}

void drawDied() {
  u8g2.setFont(u8g2_font_9x15_tr);
  u8g2.drawStr((128 - 9 * 8) / 2, 10, "Tia Died");
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.drawStr((128 - u8g2.getStrWidth(deathReason)) / 2, 20, deathReason);
  u8g2.drawXBMP((128 - CROSS_WIDTH) / 2, 24, CROSS_WIDTH, CROSS_HEIGHT, cross_f0);
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
    lastHappinessDepletedAt   = aliveStartedAt;
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
      addHunger(-1);
      lastHungerDepletedAt = now;
      if (!hungerAlertFired && hungerLevel < HUNGER_ALERT_THRESHOLD) {
        Serial.print("WARNING: hunger below "); Serial.print(HUNGER_ALERT_THRESHOLD); Serial.println("%");
        hungerAlertFired = true;
      }
    }
    if (now - lastHappinessDepletedAt >= (60000UL / HAPPINESS_PASSIVE_DRAIN_PER_MIN)) {
      addHappiness(-1);
      lastHappinessDepletedAt = now;
      if (!happinessAlertFired && happinessLevel < HAPPINESS_ALERT_THRESHOLD) {
        Serial.print("WARNING: happiness below "); Serial.print(HAPPINESS_ALERT_THRESHOLD); Serial.println("%");
        happinessAlertFired = true;
      }
    }
    if (eepLevel > HAPPINESS_SLEEPY_THRESHOLD && now - lastHappinessSleepyAt >= (60000UL / HAPPINESS_SLEEPY_DRAIN_PER_MIN)) {
      addHappiness(-1);
      lastHappinessSleepyAt = now;
    }
    if (hungerLevel == 0 || happinessLevel == 0) {
      deathReason = (hungerLevel == 0) ? "Died of Starvation" : "Died of Sadness";
      Serial.print("Tia died: "); Serial.println(deathReason);
      gameState = GameState::DIED;
    }
  }

  if (isInGameLoop() && middleBtn) {
    menuReturnState   = gameState;
    menuSelectedIndex = 0;
    activeMenuOptions     = (gameState == GameState::SLEEPING) ? MENU_OPTIONS_SLEEPING : MENU_OPTIONS_ALIVE;
    activeMenuOptionCount = (gameState == GameState::SLEEPING) ? 2 : 5;
    menuScrollOffset      = 0;
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
        lastHungerDepletedAt    = aliveStartedAt;
        lastHappinessDepletedAt = aliveStartedAt;
        lastHappinessSleepyAt   = aliveStartedAt;
        lastEepDepletedAt       = aliveStartedAt;
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
        addHappiness(HAPPINESS_SLEEP_BONUS);
        aliveStartedAt        = millis();
        lastHungerDepletedAt  = aliveStartedAt;
        lastHappinessDepletedAt = aliveStartedAt;
        lastHappinessSleepyAt = aliveStartedAt;
        lastEepDepletedAt     = aliveStartedAt;
        gameState             = GameState::ALIVE;
      }
      break;
    }
    case GameState::MENU: {
      if (leftBtn) {
        menuSelectedIndex = (menuSelectedIndex + activeMenuOptionCount - 1) % activeMenuOptionCount;
        if (menuSelectedIndex < menuScrollOffset) menuScrollOffset = menuSelectedIndex;
        else if (menuSelectedIndex >= menuScrollOffset + 4) menuScrollOffset = menuSelectedIndex - 3;
      }
      if (rightBtn) {
        menuSelectedIndex = (menuSelectedIndex + 1) % activeMenuOptionCount;
        if (menuSelectedIndex < menuScrollOffset) menuScrollOffset = 0;
        else if (menuSelectedIndex >= menuScrollOffset + 4) menuScrollOffset = menuSelectedIndex - 3;
      }
      if (middleBtn) {
        switch (menuSelectedIndex) {
          case 0: // Close menu
            gameState = menuReturnState;
            break;
          case 1: // Go to eep / Wake up
            if (menuReturnState == GameState::SLEEPING) {
              addHappiness(HAPPINESS_SLEEP_BONUS);
              aliveStartedAt          = millis();
              lastHungerDepletedAt    = aliveStartedAt;
              lastHappinessDepletedAt = aliveStartedAt;
              lastHappinessSleepyAt   = aliveStartedAt;
              lastEepDepletedAt       = aliveStartedAt;
              gameState               = GameState::ALIVE;
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
          case 3: // Pat the Tia
            interactingLabel     = "Pat the Tia";
            interactingBonus     = HAPPINESS_PAT_BONUS;
            interactingStartedAt = millis();
            gameState            = GameState::INTERACTING;
            break;
          case 4: // Play with Kitties
            interactingLabel     = "Play with Kitties";
            interactingBonus     = HAPPINESS_KITTIES_BONUS;
            interactingStartedAt = millis();
            gameState            = GameState::INTERACTING;
            break;
        }
      }
      break;
    }
    case GameState::FEEDING:
      if (millis() - feedingStartedAt >= FEEDING_DURATION_MS) {
        addHunger(FEED_HUNGER_BONUS);
        addHappiness(HAPPINESS_FEED_BONUS);
        aliveStartedAt          = millis();
        lastHungerDepletedAt    = aliveStartedAt;
        lastHappinessDepletedAt = aliveStartedAt;
        lastHappinessSleepyAt   = aliveStartedAt;
        lastEepDepletedAt       = aliveStartedAt;
        gameState    = GameState::ALIVE;
      }
      break;
    case GameState::INTERACTING: {
      uint16_t interactDuration = (interactingLabel == "Pat the Tia") ? 4000 : INTERACTION_DURATION_MS;
      if (millis() - interactingStartedAt >= interactDuration) {
        addHappiness(interactingBonus);
        aliveStartedAt          = millis();
        lastHungerDepletedAt    = aliveStartedAt;
        lastHappinessDepletedAt = aliveStartedAt;
        lastHappinessSleepyAt   = aliveStartedAt;
        lastEepDepletedAt       = aliveStartedAt;
        gameState               = GameState::ALIVE;
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
    case GameState::SLEEPING: drawSleeping(); break;
    case GameState::MENU:     drawMenu();     break;
    case GameState::FEEDING:     drawFeeding();     break;
    case GameState::INTERACTING: drawInteracting(); break;
    case GameState::DIED:     drawDied();     break;
  }

  u8g2.sendBuffer();
}
