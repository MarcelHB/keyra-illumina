// SPDX-License-Identifier: MIT

#include <algorithm>
#include <mutex>
#include <optional>
#include <random>
#include <thread>
#include <string>
#include <vector>

#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#if defined(USE_TRACY)
  #define TRACY_ENABLE 1
  #include <tracy/Tracy.hpp>
  #define TRACY(x) x
#else
  #define TRACY(x)
#endif

struct Config {
  uint32_t windowWidth = 640;
  uint32_t windowHeight = 480;
  bool vsync = true;
};

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

struct GameState {
  std::mutex stateMutex;
  GameControlAction controlAction = GameControlAction::NONE;
  GameEvents events;
  SDL_Time totalTime = 0;
  TurnState turn;
  size_t currentKeyPresses = 0;
  bool drawThreadRedraw = true;
  bool drawThreadRestart = true;
  bool drawThreadShutdown = false;
};

struct RenderState {
  SDL_Renderer *sdlRenderer = nullptr;
  SDL_Window *sdlWindow = nullptr;
  SDL_Texture *backgroundLayer = nullptr;
  SDL_Texture *hitLayer = nullptr;
  SDL_Texture *evaluationLayer = nullptr;

  SDL_Gamepad *gamepad = nullptr;
  SDL_JoystickID gamepadID = 0;
  int16_t lastRightAxisMotion = 0;
  bool lastRightAxisNeedsReset = false;
};

struct Difficulty {
  size_t timeframeLong;
  size_t timeframeShort;
  size_t numEvents;
  std::uniform_int_distribution<size_t> pauseDistrib;
  std::uniform_int_distribution<size_t> numDistrib;
  std::uniform_int_distribution<size_t> repeatDistrib;
};

constexpr size_t TIMEFRAME_LONG = 220'000'000;
constexpr size_t TIMEFRAME_LONG_EASY = 500'000'000;
constexpr size_t TIMEFRAME_SHORT = 150'000'000;
constexpr size_t TIMEFRAME_SHORT_EASY = 350'000'000;

Difficulty easyDifficulty {
    .timeframeLong = TIMEFRAME_LONG_EASY
  , .timeframeShort = TIMEFRAME_SHORT_EASY
  , .numEvents = 10
  , .pauseDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG_EASY + 150'000'000, 4'000'000'000}
  , .numDistrib = std::uniform_int_distribution<size_t>{0, 5}
  , .repeatDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG_EASY + 150'000'000, 1'000'000'000}
};

Difficulty normalDifficulty {
    .timeframeLong = TIMEFRAME_LONG
  , .timeframeShort = TIMEFRAME_SHORT
  , .numEvents = 10
  , .pauseDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG + 100'000'000, 3'000'000'000}
  , .numDistrib = std::uniform_int_distribution<size_t>{0, 6}
  , .repeatDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG + 100'000'000, 1'000'000'000}
};

Difficulty hardDifficulty {
    .timeframeLong = TIMEFRAME_LONG
  , .timeframeShort = TIMEFRAME_SHORT
  , .numEvents = 15
  , .pauseDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG + 100'000'000, 2'000'000'000}
  , .numDistrib = std::uniform_int_distribution<size_t>{0, 10}
  , .repeatDistrib = std::uniform_int_distribution<size_t>{TIMEFRAME_LONG + 100'000'000, 1'000'000'000}
};

Difficulty *currentDifficulty = &normalDifficulty;

constexpr size_t NUM_INPUT_PREALLOC = 100;

GameState game;

RenderState render;
Config config;

std::thread drawThread;
std::random_device randomDevice;
std::mt19937_64 randomGenerator{randomDevice()};

bool decreaseDifficulty();
void drawEvaluation();
void drawFrame();
bool increaseDifficulty();
void newGame();
void paintTheGame();
void parseConfig(int, char**);
void resetHitLayer();
void resetTurn();
int shutdown(bool error);

void processGamepadAxisMoution(const SDL_GamepadAxisEvent&);
void processGamepadButtonDown(const SDL_GamepadButtonEvent&);
void processKeyDown(const SDL_KeyboardEvent&);
void processKeyUp();

int main(int argc, char **argv) {
  parseConfig(argc, argv);
  if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    return shutdown(true);
  }

  auto props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Keyra Illumina");
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
  render.sdlWindow = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);

  if (render.sdlWindow == nullptr) {
    return shutdown(true);
  }

#ifndef WIN32
  // We must avoid OpenGL as it won't work with a drawing thread
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
#endif

  props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, render.sdlWindow);
  if (config.vsync) {
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);
  }
  render.sdlRenderer = SDL_CreateRendererWithProperties(props);
  SDL_DestroyProperties(props);

  if (render.sdlRenderer == nullptr) {
    return shutdown(true);
  }

  // Because of wayland, we need to draw something before we can wait for events
  SDL_SetRenderTarget(render.sdlRenderer, nullptr);
  SDL_SetRenderDrawColor(render.sdlRenderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(render.sdlRenderer);
  SDL_RenderPresent(render.sdlRenderer);

  // also Wayland, otherwise returns 1x1 below
  SDL_SyncWindow(render.sdlWindow);

  SDL_Rect display;
  SDL_GetWindowSafeArea(render.sdlWindow, &display);
  config.windowWidth = display.w;
  config.windowHeight = display.h;

  SDL_HideCursor();

  newGame();
  drawThread = std::thread(drawFrame);

  SDL_Event event;
  while (true) {
    if (SDL_PollEvent(&event) != 1) {
      // Win: ~500µs, Linux: ~50µs
      SDL_DelayNS(1000);
      continue;
    }

    game.controlAction = GameControlAction::NONE;
    if (event.type == SDL_EVENT_KEY_DOWN) {
      processKeyDown(event.key);
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
      if (render.gamepad != nullptr) {
        continue;
      }

      render.gamepad = SDL_OpenGamepad(event.gdevice.which);
      render.gamepadID = event.gdevice.which;
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
      if (render.gamepad == nullptr || render.gamepadID != event.gdevice.which) {
        continue;
      }

      SDL_CloseGamepad(render.gamepad);
      render.gamepad = nullptr;
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
      processGamepadButtonDown(event.gbutton);
    } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
      processGamepadAxisMoution(event.gaxis);
    } else if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
      processKeyUp();
    } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      game.controlAction = GameControlAction::QUIT;
    }

    if (game.controlAction == GameControlAction::QUIT) {
      game.drawThreadShutdown = true;
      break;
    } else if (game.controlAction == GameControlAction::RESET) {
      game.drawThreadRedraw = true;
      newGame();
    } else if (game.controlAction == GameControlAction::RESTART) {
      {
        std::lock_guard<std::mutex> lock {game.stateMutex};
        game.drawThreadRestart = true;
        resetTurn();
      }
    }
  }

  drawThread.join();

  return shutdown(false);
}

bool decreaseDifficulty() {
  if (currentDifficulty == &easyDifficulty) {
    return false;
  }

  if (currentDifficulty == &normalDifficulty) {
    currentDifficulty = &easyDifficulty;
  } else {
    currentDifficulty = &normalDifficulty;
  }

  return true;
}

void drawEvaluation() {
  if (render.evaluationLayer == nullptr) {
    render.evaluationLayer =
      SDL_CreateTexture(
          render.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  SDL_SetRenderTarget(render.sdlRenderer, render.evaluationLayer);

  auto timeToX = [](SDL_Time time) -> uint16_t {
    return (time * config.windowWidth) / game.totalTime;
  };

  std::optional<size_t> lastCheckedInput;

  for (auto& event : game.events) {
    auto startAt = 0;
    if (lastCheckedInput) {
      startAt = (*lastCheckedInput);
    }

    SDL_Time firstNs = event.from, firstNs2 = 0;
    SDL_Time lastNs = firstNs + currentDifficulty->timeframeLong, lastNs2 = 0;

    if (event.action == Action::DODGE_PARRY) {
      firstNs2 = firstNs + (currentDifficulty->timeframeLong - currentDifficulty->timeframeShort) / 2;
      lastNs2 = firstNs2 + currentDifficulty->timeframeShort;
    }

    size_t i = 0;
    bool success = false, betterSuccess = false;

    for (auto input = game.turn.player.cbegin() + startAt; input != game.turn.player.cend(); ++input) {
      auto inputOn = input->on - game.turn.startedAt;
      // Too early, don't care
      if (inputOn < firstNs) {
        i++;
        continue;
      }

      // Nothing for this, go on for next event
      if (inputOn > lastNs) {
        break;
      }

      if (event.action == Action::PARRY2) {
        success = input->reaction == Reaction::PARRY2 && inputOn <= lastNs;
      } else if (event.action == Action::JUMP) {
        success = input->reaction == Reaction::JUMP && inputOn <= lastNs;
      } else {
        if (input->reaction == Reaction::PARRY) {
          betterSuccess = inputOn >= firstNs2 && inputOn <= lastNs2;
        } else {
          success = input->reaction == Reaction::DODGE && inputOn <= lastNs;
        }
      }

      i++;
      if (success || betterSuccess) {
        break;
      }
    }

    uint8_t n = 0;
    uint16_t x = timeToX(firstNs) - 10;
    uint16_t w = timeToX(lastNs) - x + 10;
    auto segmentHeight = config.windowHeight / 6;

    if (betterSuccess) {
      n = 1;
      x = timeToX(firstNs2) - 10;
      w = timeToX(lastNs2) - x + 10;
    } else if (event.action == Action::PARRY2) {
      n = 2;
    } else if (event.action == Action::JUMP) {
      n = 3;
    }

    if (success || betterSuccess) {
      SDL_SetRenderDrawColor(render.sdlRenderer, 0, 200, 0, SDL_ALPHA_OPAQUE);
    } else {
      SDL_SetRenderDrawColor(render.sdlRenderer, 210, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_FRect rect;
    rect.x = x;
    rect.w = w;
    rect.y = (1 + n) * segmentHeight + segmentHeight / 4 - 10;
    rect.h = segmentHeight / 2 + 20;
    SDL_RenderFillRect(render.sdlRenderer, &rect);

    lastCheckedInput = {lastCheckedInput.value_or(0) + i};
  }
}

void drawFrame() {
  while (true) {
    TRACY(ZoneScoped);
    SDL_Time now = SDL_GetTicksNS();

    if (game.drawThreadShutdown) {
      break;
    } else if (game.drawThreadRedraw) {
      paintTheGame();
      game.drawThreadRedraw = false;
    } else if (game.drawThreadRestart) {
      resetHitLayer();
      game.drawThreadRestart = false;
    }

    std::vector<Input> newInput;
    {
      std::lock_guard<std::mutex> lock {game.stateMutex};
      size_t alreadyIn = 0;
      if (game.turn.drawnUntil) {
        alreadyIn = (*game.turn.drawnUntil) + 1;
      }

      newInput.resize(game.turn.player.size() - alreadyIn);

      std::copy(
          game.turn.player.cbegin() + alreadyIn
        , game.turn.player.cend()
        , newInput.begin()
      );

      if (!newInput.empty()) {
        game.turn.drawnUntil = {alreadyIn};
      }
    }

    if (!newInput.empty()) {
      SDL_SetRenderTarget(render.sdlRenderer, render.hitLayer);

      for (const auto& input : newInput) {
        auto x = ((input.on - game.turn.startedAt) * config.windowWidth) / game.totalTime;
        if (input.reaction == Reaction::LOSS || input.reaction == Reaction::BAD) {
          SDL_SetRenderDrawColor(render.sdlRenderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        } else {
          SDL_SetRenderDrawColor(render.sdlRenderer, 180, 180, 180, SDL_ALPHA_OPAQUE);
        }

        SDL_RenderLine(render.sdlRenderer, x, 0, x, config.windowHeight);
      }
    }

    SDL_SetRenderTarget(render.sdlRenderer, nullptr);
    SDL_SetRenderDrawColor(render.sdlRenderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(render.sdlRenderer);

    if (render.evaluationLayer) {
      SDL_RenderTexture(render.sdlRenderer, render.evaluationLayer, nullptr, nullptr);
    }
    SDL_RenderTexture(render.sdlRenderer, render.backgroundLayer, nullptr, nullptr);
    SDL_RenderTexture(render.sdlRenderer, render.hitLayer, nullptr, nullptr);

    // ruler
    if (game.turn.startedAt + game.totalTime > now) {
      auto x = ((now - game.turn.startedAt) * config.windowWidth) / game.totalTime;
      SDL_SetRenderDrawColor(render.sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
      SDL_RenderLine(render.sdlRenderer, x, 0, x, config.windowHeight);
    } else {
      if (!config.vsync) {
        // relax when done
        SDL_Delay(1);
      }

      if (!game.turn.evaluated) {
        std::lock_guard<std::mutex> lock {game.stateMutex};
        drawEvaluation();
        game.turn.evaluated = true;
      }
    }

    SDL_RenderPresent(render.sdlRenderer);
    TRACY(FrameMark);
  }
}

bool increaseDifficulty() {
  if (currentDifficulty == &hardDifficulty) {
    return false;
  }

  if (currentDifficulty == &normalDifficulty) {
    currentDifficulty = &hardDifficulty;
  } else {
    currentDifficulty = &normalDifficulty;
  }

  return true;
}

void newGame() {
  GameEvents newEvents;
  newEvents.reserve(currentDifficulty->numEvents);

  std::uniform_int_distribution<size_t> typeDistrib {0, 9};

  size_t lastOffset = 0;
  size_t eventsLeft = currentDifficulty->numEvents;
  while (eventsLeft > 0) {
    GameEvent gameEvent;

    // time offset
    auto result = lastOffset + currentDifficulty->pauseDistrib(randomGenerator);
    gameEvent.from = lastOffset = result;

    // type
    result = typeDistrib(randomGenerator);
    if (result < 8) {
      gameEvent.action = Action::DODGE_PARRY;
    } else if (result == 8) {
      gameEvent.action = Action::PARRY2;
    } else {
      gameEvent.action = Action::JUMP;
    }

    eventsLeft -= 1;
    newEvents.emplace_back(std::move(gameEvent));

    // repeat dodge / parry in a row maybe
    if (gameEvent.action == Action::DODGE_PARRY) {
      auto followEvents = std::min(eventsLeft, currentDifficulty->numDistrib(randomGenerator));
      auto interval = currentDifficulty->repeatDistrib(randomGenerator);
      for (size_t i = 0; i < followEvents; ++i) {
        GameEvent followEvent;
        followEvent.action = gameEvent.action;
        followEvent.from = lastOffset = lastOffset + interval;
        newEvents.emplace_back(std::move(followEvent));
      }

      eventsLeft -= followEvents;
    }
  }

  {
    std::lock_guard<std::mutex> lock {game.stateMutex};
    game.events = std::move(newEvents);
    game.currentKeyPresses = 0;
    game.totalTime = game.events.back().from + 1'000'000'000;
    resetTurn();
  }
}

void paintTheGame() {
  TRACY(ZoneScoped);

  if (render.backgroundLayer == nullptr) {
    render.backgroundLayer =
      SDL_CreateTexture(
          render.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  if (render.backgroundLayer == nullptr) {
    return;
  }

  if (render.hitLayer == nullptr) {
    render.hitLayer =
      SDL_CreateTexture(
          render.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  if (render.hitLayer == nullptr) {
    return;
  }

  resetHitLayer();

  SDL_SetRenderTarget(render.sdlRenderer, render.backgroundLayer);
  SDL_SetRenderDrawColor(render.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(render.sdlRenderer);

  // On some SDL3.2.x and Vulkan, we need some initial reset draws or the first game looks bad
  for (uint8_t i = 0; i < 4; ++i) {
    SDL_RenderLine(render.sdlRenderer, 0, 0, 1, 1);
  }
  SDL_FlushRenderer(render.sdlRenderer);

  // 4 tracks + 2 margin
  SDL_SetRenderDrawColor(render.sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  auto segmentHeight = config.windowHeight / 6;
  for (size_t i = 0; i < 4; ++i) {
    auto y = (i + 1) * segmentHeight + segmentHeight / 2;
    SDL_RenderLine(render.sdlRenderer, 0, y, config.windowWidth, y);
  }

  auto timeToX = [](SDL_Time time) -> uint16_t {
    return (time * config.windowWidth) / game.totalTime;
  };

  uint8_t r = 0, g = 0, b = 0, n = 0;
  for (auto& event : game.events) {
    if (event.action == Action::DODGE_PARRY) {
      r = 125; g = 95; b = 150, n = 0;
    } else if (event.action == Action::PARRY2) {
      r = 150; g = 50; b = 70, n = 2;
    } else {
      r = 220; g = 180; b = 125, n = 3;
    }
    SDL_SetRenderDrawColor(render.sdlRenderer, r, g, b, SDL_ALPHA_OPAQUE);

    SDL_FRect rect;
    rect.x = timeToX(event.from);
    rect.w = timeToX(event.from + currentDifficulty->timeframeLong) - rect.x;
    rect.y = (1 + n) * segmentHeight + segmentHeight / 4;
    rect.h = segmentHeight / 2;
    SDL_RenderFillRect(render.sdlRenderer, &rect);

    if (event.action == Action::DODGE_PARRY) {
      rect.x += timeToX((currentDifficulty->timeframeLong - currentDifficulty->timeframeShort) / 2);
      rect.w = timeToX(currentDifficulty->timeframeShort);
      rect.y = 2 * segmentHeight + segmentHeight / 4;
      SDL_RenderFillRect(render.sdlRenderer, &rect);
    }
  }
}

void parseConfig(int argc, char **argv) {
  if (argc <= 1) {
    return;
  }

  for (int i = 1; i < argc; ++i) {
    std::string arg {argv[i]};

    if (arg == "-novsync") {
      config.vsync = false;
    }
  }
}

// RT button handling
void processGamepadAxisMoution(const SDL_GamepadAxisEvent& event) {
  TRACY(ZoneScoped);

  bool captureInput = false;
  Input input;
  input.on = event.timestamp;

  // this may be sticks: too many events on moving, discard
  if (event.axis != 5) {
    return;
  }

  // turning point or max value
  if (!render.lastRightAxisNeedsReset
      && (event.value < render.lastRightAxisMotion || event.value == std::numeric_limits<int16_t>::max())) {
    captureInput = true;
    input.reaction = Reaction::PARRY2;
    render.lastRightAxisNeedsReset = true;
  } else if (render.lastRightAxisNeedsReset && event.value == 0) {
    render.lastRightAxisNeedsReset = false;
  }

  render.lastRightAxisMotion = event.value;

  if (captureInput) {
    std::lock_guard<std::mutex> lock {game.stateMutex};
    game.turn.player.emplace_back(std::move(input));
  }
}

void processGamepadButtonDown(const SDL_GamepadButtonEvent& event) {
  TRACY(ZoneScoped);

  Input input;
  input.on = event.timestamp;

  game.controlAction = GameControlAction::NONE;
  switch (event.button) {
    case 1: // B
      input.reaction = Reaction::DODGE; break;
    case 10: // RB
      input.reaction = Reaction::PARRY; break;
    case 0: // A
      input.reaction = Reaction::JUMP; break;
    case 7: // L (M1)
      game.controlAction = GameControlAction::RESET;
      break;
    case 6: // burger
      game.controlAction = GameControlAction::RESTART;
      break;
    case 11: // arrow up
      if (increaseDifficulty()) {
        game.controlAction = GameControlAction::RESET;
      }
      break;
    case 12: // arrow down
      if (decreaseDifficulty()) {
        game.controlAction = GameControlAction::RESET;
      }
      break;
    case 4: // windows
      game.controlAction = GameControlAction::QUIT;
      break;
    default:
      input.reaction = Reaction::BAD;
  }

  if (game.controlAction != GameControlAction::NONE) {
    return;
  }

  if (game.currentKeyPresses > 0) {
    input.reaction = Reaction::LOSS;
  }

  game.currentKeyPresses += 1;

  {
    std::lock_guard<std::mutex> lock {game.stateMutex};
    game.turn.player.emplace_back(std::move(input));
  }
}

void processKeyDown(const SDL_KeyboardEvent& event) {
  TRACY(ZoneScoped);

  Input input;
  input.on = event.timestamp;

  game.controlAction = GameControlAction::NONE;
  switch (event.key) {
    case SDLK_Q:
      input.reaction = Reaction::DODGE; break;
    case SDLK_E:
      input.reaction = Reaction::PARRY; break;
    case SDLK_W:
      input.reaction = Reaction::PARRY2; break;
    case SDLK_SPACE:
      input.reaction = Reaction::JUMP; break;
    case SDLK_R:
      game.controlAction = GameControlAction::RESET;
      break;
    case SDLK_S:
      game.controlAction = GameControlAction::RESTART;
      break;
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
      if (increaseDifficulty()) {
        game.controlAction = GameControlAction::RESET;
      }
      break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
      if (decreaseDifficulty()) {
        game.controlAction = GameControlAction::RESET;
      }
      break;
    case SDLK_ESCAPE:
      game.controlAction = GameControlAction::QUIT;
      break;
    default:
      input.reaction = Reaction::BAD;
  }

  if (game.controlAction != GameControlAction::NONE) {
    return;
  }

  if (game.currentKeyPresses > 0) {
    input.reaction = Reaction::LOSS;
  }

  game.currentKeyPresses += 1;

  {
    std::lock_guard<std::mutex> lock {game.stateMutex};
    game.turn.player.emplace_back(std::move(input));
  }
}

void processKeyUp() {
  if (game.currentKeyPresses > 0) {
    game.currentKeyPresses -= 1;
  }
}

void resetHitLayer() {
  SDL_SetRenderTarget(render.sdlRenderer, render.hitLayer);
  SDL_SetRenderDrawColor(render.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(render.sdlRenderer);

  SDL_SetRenderTarget(render.sdlRenderer, render.evaluationLayer);
  SDL_SetRenderDrawColor(render.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(render.sdlRenderer);
}

// call when locked
void resetTurn() {
  game.turn.startedAt = SDL_GetTicksNS();
  game.turn.player.clear();
  game.turn.drawnUntil = {};
  game.turn.evaluated = false;
}

int shutdown(bool error) {
  if (render.gamepad != nullptr) {
    SDL_CloseGamepad(render.gamepad);
    render.gamepad = nullptr;
  }

  if (render.backgroundLayer != nullptr) {
    SDL_DestroyTexture(render.backgroundLayer);
    render.backgroundLayer = nullptr;
  }

  if (render.hitLayer != nullptr) {
    SDL_DestroyTexture(render.hitLayer);
    render.hitLayer = nullptr;
  }

  if (render.evaluationLayer != nullptr) {
    SDL_DestroyTexture(render.evaluationLayer);
    render.evaluationLayer = nullptr;
  }

  if (render.sdlRenderer != nullptr) {
    SDL_DestroyRenderer(render.sdlRenderer);
    render.sdlRenderer = nullptr;
  }

  if (render.sdlWindow != nullptr) {
    SDL_DestroyWindow(render.sdlWindow);
    render.sdlWindow = nullptr;
  }

  SDL_Quit();

  return error ? 1 : 0;
}
