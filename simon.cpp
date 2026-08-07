// SPDX-License-Identifier: MIT
#include <thread>
#include <vector>

#include <SDL3/SDL_main.h>

#include "setup.h"
#include "common.h"

struct ScheduledEvent {
  enum class Type { PAUSE, PARRY, JUMP, PARRY2 };

  Type type;
  SDL_Time offset;

  ScheduledEvent (Type t, SDL_Time o) : type(t), offset(o) {}
};

using Schedule = std::vector<std::vector<ScheduledEvent>>;

// Reverse engineered from video material
Schedule phases = {
  // phase 1
  {
    // brawl (sword in the ground)
      { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 15 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 21 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 16 * static_cast<SDL_Time>(100'000'000) }
    // short combo
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 15 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::JUMP, 14 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY2, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 13 * static_cast<SDL_Time>(100'000'000) }
    // powerful combo
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 14 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    // long combo
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
  // phase 2
  }, {
    // short combo (2)
      { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::JUMP, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY2, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // powerful combo (2)
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 6 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // long combo (2)
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 4 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // sword of Lumière
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 6 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 9 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 15 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 14 * static_cast<SDL_Time>(100'000'000) }
    // lightspeed attack
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 23 * static_cast<SDL_Time>(100'000'000) }
  // phase 3
  }, {
    // short combo (3)
      { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::JUMP, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY2, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // powerful combo (3)
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // long combo (3)
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 5 * static_cast<SDL_Time>(100'000'000) }
    // sword of Lumière
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 6 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 9 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 15 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 14 * static_cast<SDL_Time>(100'000'000) }
    // lightspeed attack
    , { ScheduledEvent::Type::PAUSE, 0 }
    , { ScheduledEvent::Type::PARRY, 0 }
    , { ScheduledEvent::Type::PARRY, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 8 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, static_cast<SDL_Time>(1'000'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 12 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 7 * static_cast<SDL_Time>(100'000'000) }
    , { ScheduledEvent::Type::PARRY, 23 * static_cast<SDL_Time>(100'000'000) }
  }
};

GameState gameState;
RenderState renderState;
Config config;

std::thread drawThread;

uint8_t currentPhase = 0;

int main(int argc, char **argv) {
  parseConfig(config, argc, argv);
  if (!setupSDL(renderState, config)) {
    return shutdownSDL(renderState, true);
  }

  gameState.gameMode = GameMode::SIMON;
  newGame();

  drawThread = std::thread(drawFrame, std::ref(gameState), std::ref(renderState), std::ref(config));
  mainLoop(gameState, renderState);
  drawThread.join();

  return shutdownSDL(renderState, false);
}

bool increaseDifficulty(GameState&) {
  if (currentPhase < 2) {
    currentPhase += 1;
    return true;
  }

  return false;
}

bool decreaseDifficulty(GameState&) {
  if (currentPhase > 0) {
    currentPhase -= 1;
    return true;
  }

  return false;
}

void newGame() {
  auto numEvents = phases[currentPhase].size();

  GameEvents newEvents;
  newEvents.reserve(numEvents);

  SDL_Time offset = 0;
  bool firstPause = true;

  for (auto& scheduledEvent : phases[currentPhase]) {
    if (scheduledEvent.type == ScheduledEvent::Type::PAUSE) {
      offset += (firstPause ? 1 : 2) * 1'000'000'000;
      firstPause = false;
      continue;
    }

    auto& event = newEvents.emplace_back();
    event.from = offset + scheduledEvent.offset;
    offset += scheduledEvent.offset;

    if (scheduledEvent.type == ScheduledEvent::Type::PARRY) {
      event.action = Action::DODGE_PARRY;
    } else if (scheduledEvent.type == ScheduledEvent::Type::JUMP) {
      event.action = Action::JUMP;
    } else if (scheduledEvent.type == ScheduledEvent::Type::PARRY2) {
      event.action = Action::PARRY2;
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

    SDL_Time firstNs = event.from;
    SDL_Time lastNs = firstNs + gameState.currentDifficulty->timeframeLong;

    if (event.action == Action::DODGE_PARRY) {
      firstNs = firstNs + (gameState.currentDifficulty->timeframeLong - gameState.currentDifficulty->timeframeShort) / 2;
      lastNs = firstNs + gameState.currentDifficulty->timeframeShort;
    }

    size_t i = 0;
    bool success = false;

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
      } else {
        if (input->reaction == Reaction::PARRY) {
          success = inputOn >= firstNs && inputOn <= lastNs;
        }
      }

      i++;
      if (success) {
        break;
      }
    }

    uint8_t n = 0;
    uint16_t x = timeToX(firstNs) - 10;
    uint16_t w = timeToX(lastNs) - x + 10;
    auto segmentHeight = config.windowHeight / 5;

    if (event.action == Action::PARRY2) {
      n = 1;
    } else if (event.action == Action::JUMP) {
      n = 2;
    }

    if (success) {
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
