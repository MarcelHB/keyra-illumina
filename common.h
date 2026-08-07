// SPDX-License-Identifier: MIT

#ifndef H_COMMON
#define H_COMMON

#include <mutex>
#include <optional>
#include <random>
#include <vector>

#include <SDL3/SDL.h>

#include "setup.h"

enum class Reaction {
    NONE
  , DODGE   // Q / B
  , PARRY   // E / RB
  , PARRY2  // W / RT
  , JUMP    // SPACE / A
  , BAD     // something else
  , LOSS    // something lost
};

enum class Action {
    DODGE_PARRY
  , DANSEUSE
  , PARRY2
  , JUMP
};

struct GameEvent {
  Action action;
  SDL_Time from;
};

struct Input {
  Reaction reaction = Reaction::NONE;
  SDL_Time on;
};

struct TurnState {
  std::vector<Input> player;
  SDL_Time startedAt = 0;
  std::optional<size_t> drawnUntil;
  bool evaluated = false;
};

enum class GameControlAction {
    NONE
  , QUIT
  , RESET
  , RESTART
};

using GameEvents = std::vector<GameEvent>;

struct Difficulty {
  size_t timeframeLong;
  size_t timeframeShort;
  size_t numEvents;
  std::uniform_int_distribution<size_t> pauseDistrib;
  std::uniform_int_distribution<size_t> numDistrib;
  std::uniform_int_distribution<size_t> repeatDistrib;
};

extern Difficulty easyDifficulty;
extern Difficulty normalDifficulty;
extern Difficulty hardDifficulty;

constexpr size_t NUM_INPUT_PREALLOC = 100;

enum class GameMode {
    RANDOM
  , SIMON
};

struct GameState {
  Difficulty* currentDifficulty = &normalDifficulty;
  GameMode gameMode = GameMode::RANDOM;
  std::mutex stateMutex;
  GameControlAction controlAction = GameControlAction::NONE;
  GameEvents events;
  SDL_Time totalTime = 0;
  TurnState turn;
  size_t currentKeyPresses = 0;
  bool drawThreadRedraw = true;
  bool drawThreadRestart = true;
  bool drawThreadShutdown = false;

  GameState() {
    events.reserve(NUM_INPUT_PREALLOC);
  }
};

constexpr size_t TIMEFRAME_LONG = 220'000'000;
constexpr size_t TIMEFRAME_LONG_EASY = 500'000'000;
constexpr size_t TIMEFRAME_SHORT = 150'000'000;
constexpr size_t TIMEFRAME_SHORT_EASY = 350'000'000;

void drawFrame(GameState&, RenderState&, Config&);
void mainLoop(GameState&, RenderState&);
void paintTheGame(GameState&, RenderState&, Config&);
void resetHitLayer(RenderState&);
void resetTurn(GameState&);

void processGamepadAxisMoution(GameState&, RenderState&, const SDL_GamepadAxisEvent&);
void processGamepadButtonDown(GameState&, const SDL_GamepadButtonEvent&);
void processKeyDown(GameState&, const SDL_KeyboardEvent&);
void processKeyUp(GameState&);

// to be implemented by game
bool increaseDifficulty(GameState&);
bool decreaseDifficulty(GameState&);

void drawEvaluation();
void newGame();

#endif
