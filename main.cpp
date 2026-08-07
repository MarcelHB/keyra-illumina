// SPDX-License-Identifier: MIT

#include <optional>
#include <random>
#include <thread>

#include <SDL3/SDL_main.h>

#include "setup.h"
#include "common.h"

GameState gameState;
RenderState renderState;
Config config;

std::thread drawThread;
std::random_device randomDevice;
std::mt19937_64 randomGenerator{randomDevice()};

int main(int argc, char **argv) {
  parseConfig(config, argc, argv);
  if (!setupSDL(renderState, config)) {
    return shutdownSDL(renderState, true);
  }

  newGame();
  drawThread = std::thread(drawFrame, std::ref(gameState), std::ref(renderState), std::ref(config));
  mainLoop(gameState, renderState);
  drawThread.join();

  return shutdownSDL(renderState, false);
}

void drawEvaluation() {
  if (renderState.evaluationLayer == nullptr) {
    renderState.evaluationLayer =
      SDL_CreateTexture(
          renderState.sdlRenderer
        , SDL_PIXELFORMAT_ARGB8888
        , SDL_TEXTUREACCESS_TARGET
        , config.windowWidth
        , config.windowHeight
      );
  }

  SDL_SetRenderTarget(renderState.sdlRenderer, renderState.evaluationLayer);

  auto timeToX = [](SDL_Time time) -> uint16_t {
    return (time * config.windowWidth) / gameState.totalTime;
  };

  std::optional<size_t> lastCheckedInput;

  for (auto& event : gameState.events) {
    auto startAt = 0;
    if (lastCheckedInput) {
      startAt = (*lastCheckedInput);
    }

    SDL_Time firstNs = event.from, firstNs2 = 0;
    SDL_Time lastNs = firstNs + gameState.currentDifficulty->timeframeLong, lastNs2 = 0;

    if (event.action == Action::DODGE_PARRY) {
      firstNs2 = firstNs + (gameState.currentDifficulty->timeframeLong - gameState.currentDifficulty->timeframeShort) / 2;
      lastNs2 = firstNs2 + gameState.currentDifficulty->timeframeShort;
    }

    size_t i = 0;
    bool success = false, betterSuccess = false, prevDanseuseHit = false;

    for (auto input = gameState.turn.player.cbegin() + startAt; input != gameState.turn.player.cend(); ++input) {
      auto inputOn = input->on - gameState.turn.startedAt;
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
      } else if (event.action == Action::DANSEUSE) {
        if (prevDanseuseHit) {
          success =
            (input->reaction == Reaction::DODGE || input->reaction == Reaction::PARRY)
              && inputOn <= lastNs;
          prevDanseuseHit = false;
        } else {
          prevDanseuseHit =
            (input->reaction == Reaction::DODGE || input->reaction == Reaction::PARRY)
              && inputOn <= lastNs;
        }
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
      SDL_SetRenderDrawColor(renderState.sdlRenderer, 0, 200, 0, SDL_ALPHA_OPAQUE);
    } else {
      SDL_SetRenderDrawColor(renderState.sdlRenderer, 210, 0, 0, SDL_ALPHA_OPAQUE);
    }

    SDL_FRect rect;
    rect.x = x;
    rect.w = w;
    rect.y = (1 + n) * segmentHeight + segmentHeight / 4 - 10;
    rect.h = segmentHeight / 2 + 20;
    SDL_RenderFillRect(renderState.sdlRenderer, &rect);

    lastCheckedInput = {lastCheckedInput.value_or(0) + i};
  }
}

void newGame() {
  GameEvents newEvents;
  newEvents.reserve(gameState.currentDifficulty->numEvents);

  std::uniform_int_distribution<size_t> typeDistrib {0, 9};

  size_t lastOffset = 0;
  size_t eventsLeft = gameState.currentDifficulty->numEvents;
  while (eventsLeft > 0) {
    auto& gameEvent = newEvents.emplace_back();

    // time offset
    auto result = lastOffset + gameState.currentDifficulty->pauseDistrib(randomGenerator);
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

    // repeat dodge / parry in a row maybe
    if (gameEvent.action == Action::DODGE_PARRY) {
      auto followEvents = std::min(eventsLeft, gameState.currentDifficulty->numDistrib(randomGenerator));
      auto interval = gameState.currentDifficulty->repeatDistrib(randomGenerator);

      // Danseuse very-fast double case, treat as one
      result = typeDistrib(randomGenerator);
      if (followEvents == 1 && result <= 1) {
        gameEvent.action = Action::DANSEUSE;
        followEvents = 0;
      }

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
    std::lock_guard<std::mutex> lock {gameState.stateMutex};
    gameState.events = std::move(newEvents);
    gameState.currentKeyPresses = 0;
    gameState.totalTime = gameState.events.back().from + 1'000'000'000;
    resetTurn(gameState);
  }
}

bool increaseDifficulty(GameState& gameState) {
  if (gameState.currentDifficulty == &hardDifficulty) {
    return false;
  }

  if (gameState.currentDifficulty == &normalDifficulty) {
    gameState.currentDifficulty = &hardDifficulty;
  } else {
    gameState.currentDifficulty = &normalDifficulty;
  }

  return true;
}

bool decreaseDifficulty(GameState& gameState) {
  if (gameState.currentDifficulty == &easyDifficulty) {
    return false;
  }

  if (gameState.currentDifficulty == &normalDifficulty) {
    gameState.currentDifficulty = &easyDifficulty;
  } else {
    gameState.currentDifficulty = &normalDifficulty;
  }

  return true;
}
