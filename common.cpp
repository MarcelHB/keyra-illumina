// SPDX-License-Identifier: MIT

#include <random>

#include "common.h"

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

void mainLoop(GameState& gameState, RenderState& renderState) {
  SDL_Event event;
  while (true) {
    if (SDL_PollEvent(&event) != 1) {
      // Win: ~500µs, Linux: ~50µs
      SDL_DelayNS(1000);
      continue;
    }

    gameState.controlAction = GameControlAction::NONE;
    if (event.type == SDL_EVENT_KEY_DOWN) {
      processKeyDown(gameState, event.key);
    } else if (event.type == SDL_EVENT_GAMEPAD_ADDED) {
      if (renderState.gamepad != nullptr) {
        continue;
      }

      renderState.gamepad = SDL_OpenGamepad(event.gdevice.which);
      renderState.gamepadID = event.gdevice.which;
    } else if (event.type == SDL_EVENT_GAMEPAD_REMOVED) {
      if (renderState.gamepad == nullptr || renderState.gamepadID != event.gdevice.which) {
        continue;
      }

      SDL_CloseGamepad(renderState.gamepad);
      renderState.gamepad = nullptr;
    } else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
      processGamepadButtonDown(gameState, event.gbutton);
    } else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION) {
      processGamepadAxisMoution(gameState, renderState, event.gaxis);
    } else if (event.type == SDL_EVENT_KEY_UP || event.type == SDL_EVENT_GAMEPAD_BUTTON_UP) {
      processKeyUp(gameState);
    } else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
      gameState.controlAction = GameControlAction::QUIT;
    }

    if (gameState.controlAction == GameControlAction::QUIT) {
      gameState.drawThreadShutdown = true;
      break;
    } else if (gameState.controlAction == GameControlAction::RESET) {
      gameState.drawThreadRedraw = true;
      newGame();
    } else if (gameState.controlAction == GameControlAction::RESTART) {
      {
        std::lock_guard<std::mutex> lock {gameState.stateMutex};
        gameState.drawThreadRestart = true;
        resetTurn(gameState);
      }
    }
  }
}

void drawFrame(GameState& gameState, RenderState& renderState, Config& config) {
  while (true) {
    TRACY(ZoneScoped);
    SDL_Time now = SDL_GetTicksNS();

    if (gameState.drawThreadShutdown) {
      break;
    } else if (gameState.drawThreadRedraw) {
      paintTheGame(gameState, renderState, config);
      gameState.drawThreadRedraw = false;
    } else if (gameState.drawThreadRestart) {
      resetHitLayer(renderState);
      gameState.drawThreadRestart = false;
    }

    std::vector<Input> newInput;
    {
      std::lock_guard<std::mutex> lock {gameState.stateMutex};
      size_t alreadyIn = 0;
      if (gameState.turn.drawnUntil) {
        alreadyIn = (*gameState.turn.drawnUntil) + 1;
      }

      newInput.resize(gameState.turn.player.size() - alreadyIn);

      std::copy(
          gameState.turn.player.cbegin() + alreadyIn
        , gameState.turn.player.cend()
        , newInput.begin()
      );

      if (!newInput.empty()) {
        gameState.turn.drawnUntil = {alreadyIn};
      }
    }

    if (!newInput.empty()) {
      SDL_SetRenderTarget(renderState.sdlRenderer, renderState.hitLayer);

      for (const auto& input : newInput) {
        auto x = ((input.on - gameState.turn.startedAt) * config.windowWidth) / gameState.totalTime;
        if (input.reaction == Reaction::LOSS || input.reaction == Reaction::BAD) {
          SDL_SetRenderDrawColor(renderState.sdlRenderer, 255, 0, 0, SDL_ALPHA_OPAQUE);
        } else {
          SDL_SetRenderDrawColor(renderState.sdlRenderer, 180, 180, 180, SDL_ALPHA_OPAQUE);
        }

        SDL_RenderLine(renderState.sdlRenderer, x, 0, x, config.windowHeight);
      }
    }

    SDL_SetRenderTarget(renderState.sdlRenderer, nullptr);
    SDL_SetRenderDrawColor(renderState.sdlRenderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderState.sdlRenderer);

    if (renderState.evaluationLayer) {
      SDL_RenderTexture(renderState.sdlRenderer, renderState.evaluationLayer, nullptr, nullptr);
    }
    SDL_RenderTexture(renderState.sdlRenderer, renderState.backgroundLayer, nullptr, nullptr);
    SDL_RenderTexture(renderState.sdlRenderer, renderState.hitLayer, nullptr, nullptr);

    // ruler
    if (gameState.turn.startedAt + gameState.totalTime > now) {
      auto x = ((now - gameState.turn.startedAt) * config.windowWidth) / gameState.totalTime;
      SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
      SDL_RenderLine(renderState.sdlRenderer, x, 0, x, config.windowHeight);
    } else {
      if (!config.vsync) {
        // relax when done
        SDL_Delay(1);
      }

      if (!gameState.turn.evaluated) {
        std::lock_guard<std::mutex> lock {gameState.stateMutex};
        drawEvaluation();
        gameState.turn.evaluated = true;
      }
    }

    SDL_RenderPresent(renderState.sdlRenderer);
    TRACY(FrameMark);
  }
}

void paintTheGame(GameState& gameState, RenderState& renderState, Config& config) {
  TRACY(ZoneScoped);

  if (renderState.backgroundLayer == nullptr) {
    renderState.backgroundLayer =
      SDL_CreateTexture(
          renderState.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  if (renderState.backgroundLayer == nullptr) {
    return;
  }

  if (renderState.hitLayer == nullptr) {
    renderState.hitLayer =
      SDL_CreateTexture(
          renderState.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  if (renderState.hitLayer == nullptr) {
    return;
  }

  resetHitLayer(renderState);

  SDL_SetRenderTarget(renderState.sdlRenderer, renderState.backgroundLayer);
  SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(renderState.sdlRenderer);

  // On some SDL3.2.x and Vulkan, we need some initial reset draws or the first game looks bad
  for (uint8_t i = 0; i < 4; ++i) {
    SDL_RenderLine(renderState.sdlRenderer, 0, 0, 1, 1);
  }
  SDL_FlushRenderer(renderState.sdlRenderer);

  uint8_t tracks = gameState.gameMode == GameMode::SIMON ? 3 : 4;

  // 3/4 tracks + 2 margin
  SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
  auto segmentHeight = config.windowHeight / (tracks + 2);
  for (size_t i = 0; i < tracks; ++i) {
    auto y = (i + 1) * segmentHeight + segmentHeight / 2;
    SDL_RenderLine(renderState.sdlRenderer, 0, y, config.windowWidth, y);
  }

  auto timeToX = [&config, &gameState](SDL_Time time) -> uint16_t {
    return (time * config.windowWidth) / gameState.totalTime;
  };

  uint8_t r = 0, g = 0, b = 0, n = 0;
  for (auto& event : gameState.events) {
    if (event.action == Action::DANSEUSE) {
      r = 138; g = 157; b = 242, n = 0;
    } else if (event.action == Action::DODGE_PARRY) {
      r = 125; g = 95; b = 150, n = 0;
    } else if (event.action == Action::PARRY2) {
      r = 150; g = 50; b = 70, n = 2;
    } else {
      r = 220; g = 180; b = 125, n = 3;
    }
    SDL_SetRenderDrawColor(renderState.sdlRenderer, r, g, b, SDL_ALPHA_OPAQUE);

    SDL_FRect rect;
    rect.h = segmentHeight / 2;
    rect.x = timeToX(event.from);

    if (gameState.gameMode == GameMode::SIMON) {
      n = n - 1;
    }

    if (gameState.gameMode != GameMode::SIMON || event.action != Action::DODGE_PARRY) {
      rect.w = timeToX(event.from + gameState.currentDifficulty->timeframeLong) - rect.x;
      rect.y = (n + 1) * segmentHeight + segmentHeight / 4;
      SDL_RenderFillRect(renderState.sdlRenderer, &rect);
    }

    if (event.action == Action::DODGE_PARRY) {
      rect.x += timeToX((gameState.currentDifficulty->timeframeLong - gameState.currentDifficulty->timeframeShort) / 2);
      rect.w = timeToX(gameState.currentDifficulty->timeframeShort);
      rect.y = (gameState.gameMode == GameMode::SIMON ? 1 : 2) * segmentHeight + segmentHeight / 4;
      SDL_RenderFillRect(renderState.sdlRenderer, &rect);
    }
  }
}

void resetHitLayer(RenderState& renderState) {
  SDL_SetRenderTarget(renderState.sdlRenderer, renderState.hitLayer);
  SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(renderState.sdlRenderer);

  SDL_SetRenderTarget(renderState.sdlRenderer, renderState.evaluationLayer);
  SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 0, 0, SDL_ALPHA_TRANSPARENT);
  SDL_RenderClear(renderState.sdlRenderer);
}

// call when locked
void resetTurn(GameState& gameState) {
  gameState.turn.startedAt = SDL_GetTicksNS();
  gameState.turn.player.clear();
  gameState.turn.drawnUntil = {};
  gameState.turn.evaluated = false;
}

// RT button handling
void processGamepadAxisMoution(
    GameState& gameState
  , RenderState& renderState
  , const SDL_GamepadAxisEvent& event
) {
  TRACY(ZoneScoped);

  bool captureInput = false;
  Input input;
  input.on = event.timestamp;

  // this may be sticks: too many events on moving, discard
  if (event.axis != 5) {
    return;
  }

  // turning point or max value
  if (!renderState.lastRightAxisNeedsReset
      && (event.value < renderState.lastRightAxisMotion || event.value == std::numeric_limits<int16_t>::max())) {
    captureInput = true;
    input.reaction = Reaction::PARRY2;
    renderState.lastRightAxisNeedsReset = true;
  } else if (renderState.lastRightAxisNeedsReset && event.value == 0) {
    renderState.lastRightAxisNeedsReset = false;
  }

  renderState.lastRightAxisMotion = event.value;

  if (captureInput) {
    std::lock_guard<std::mutex> lock {gameState.stateMutex};
    gameState.turn.player.emplace_back(std::move(input));
  }
}

void processGamepadButtonDown(GameState& gameState, const SDL_GamepadButtonEvent& event) {
  TRACY(ZoneScoped);

  Input input;
  input.on = event.timestamp;

  gameState.controlAction = GameControlAction::NONE;
  switch (event.button) {
    case 1: // B
      input.reaction = gameState.gameMode == GameMode::SIMON ? Reaction::BAD : Reaction::DODGE;
      break;
    case 10: // RB
      input.reaction = Reaction::PARRY; break;
    case 0: // A
      input.reaction = Reaction::JUMP; break;
    case 7: // L (M1)
      gameState.controlAction = GameControlAction::RESET;
      break;
    case 6: // burger
      gameState.controlAction = GameControlAction::RESTART;
      break;
    case 11: // arrow up
      if (increaseDifficulty(gameState)) {
        gameState.controlAction = GameControlAction::RESET;
      }
      break;
    case 12: // arrow down
      if (decreaseDifficulty(gameState)) {
        gameState.controlAction = GameControlAction::RESET;
      }
      break;
    case 4: // windows
      gameState.controlAction = GameControlAction::QUIT;
      break;
    default:
      input.reaction = Reaction::BAD;
  }

  if (gameState.controlAction != GameControlAction::NONE) {
    return;
  }

  if (gameState.currentKeyPresses > 0) {
    input.reaction = Reaction::LOSS;
  }

  gameState.currentKeyPresses += 1;

  {
    std::lock_guard<std::mutex> lock {gameState.stateMutex};
    gameState.turn.player.emplace_back(std::move(input));
  }
}

void processKeyDown(GameState& gameState, const SDL_KeyboardEvent& event) {
  TRACY(ZoneScoped);

  Input input;
  input.on = event.timestamp;

  gameState.controlAction = GameControlAction::NONE;
  switch (event.key) {
    case SDLK_Q:
      input.reaction = gameState.gameMode == GameMode::SIMON ? Reaction::BAD : Reaction::DODGE;
      break;
    case SDLK_E:
      input.reaction = Reaction::PARRY; break;
    case SDLK_W:
      input.reaction = Reaction::PARRY2; break;
    case SDLK_SPACE:
      input.reaction = Reaction::JUMP; break;
    case SDLK_R:
      gameState.controlAction = GameControlAction::RESET;
      break;
    case SDLK_S:
      gameState.controlAction = GameControlAction::RESTART;
      break;
    case SDLK_PLUS:
    case SDLK_KP_PLUS:
      if (increaseDifficulty(gameState)) {
        gameState.controlAction = GameControlAction::RESET;
      }
      break;
    case SDLK_MINUS:
    case SDLK_KP_MINUS:
      if (decreaseDifficulty(gameState)) {
        gameState.controlAction = GameControlAction::RESET;
      }
      break;
    case SDLK_ESCAPE:
      gameState.controlAction = GameControlAction::QUIT;
      break;
    default:
      input.reaction = Reaction::BAD;
  }

  if (gameState.controlAction != GameControlAction::NONE) {
    return;
  }

  if (gameState.currentKeyPresses > 0) {
    input.reaction = Reaction::LOSS;
  }

  gameState.currentKeyPresses += 1;

  {
    std::lock_guard<std::mutex> lock {gameState.stateMutex};
    gameState.turn.player.emplace_back(std::move(input));
  }
}

void processKeyUp(GameState& gameState) {
  if (gameState.currentKeyPresses > 0) {
    gameState.currentKeyPresses -= 1;
  }
}

