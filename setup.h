// SPDX-License-Identifier: MIT

#ifndef H_SETUP
#define H_SETUP

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

void parseConfig(Config&, int argc, char **argv);
bool setupSDL(RenderState&, Config&);
int shutdownSDL(RenderState&, bool error);

#endif
