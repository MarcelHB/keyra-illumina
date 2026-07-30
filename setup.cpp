// SPDX-License-Identifier: MIT

#include <string>

#include "setup.h"

void parseConfig(Config& config, int argc, char **argv) {
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

bool setupSDL(RenderState& renderState, Config& config) {
  if (!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
    return false;
  }

  auto props = SDL_CreateProperties();
  SDL_SetStringProperty(props, SDL_PROP_WINDOW_CREATE_TITLE_STRING, "Keyra Illumina");
  SDL_SetBooleanProperty(props, SDL_PROP_WINDOW_CREATE_FULLSCREEN_BOOLEAN, true);
  renderState.sdlWindow = SDL_CreateWindowWithProperties(props);
  SDL_DestroyProperties(props);

  if (renderState.sdlWindow == nullptr) {
    return false;
  }

#ifndef WIN32
  // We must avoid OpenGL as it won't work with a drawing thread
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "vulkan");
#endif

  props = SDL_CreateProperties();
  SDL_SetPointerProperty(props, SDL_PROP_RENDERER_CREATE_WINDOW_POINTER, renderState.sdlWindow);
  if (config.vsync) {
    SDL_SetNumberProperty(props, SDL_PROP_RENDERER_CREATE_PRESENT_VSYNC_NUMBER, 1);
  }
  renderState.sdlRenderer = SDL_CreateRendererWithProperties(props);
  SDL_DestroyProperties(props);

  if (renderState.sdlRenderer == nullptr) {
    return false;
  }

  // Because of wayland, we need to draw something before we can wait for events
  SDL_SetRenderTarget(renderState.sdlRenderer, nullptr);
  SDL_SetRenderDrawColor(renderState.sdlRenderer, 255, 255, 255, SDL_ALPHA_OPAQUE);
  SDL_RenderClear(renderState.sdlRenderer);
  SDL_RenderPresent(renderState.sdlRenderer);

  // also Wayland, otherwise returns 1x1 below
  SDL_SyncWindow(renderState.sdlWindow);

  SDL_Rect display;
  SDL_GetWindowSafeArea(renderState.sdlWindow, &display);
  config.windowWidth = display.w;
  config.windowHeight = display.h;

  SDL_HideCursor();

  return true;
}

int shutdownSDL(RenderState& renderState, bool error) {
  if (renderState.gamepad != nullptr) {
    SDL_CloseGamepad(renderState.gamepad);
    renderState.gamepad = nullptr;
  }

  if (renderState.backgroundLayer != nullptr) {
    SDL_DestroyTexture(renderState.backgroundLayer);
    renderState.backgroundLayer = nullptr;
  }

  if (renderState.hitLayer != nullptr) {
    SDL_DestroyTexture(renderState.hitLayer);
    renderState.hitLayer = nullptr;
  }

  if (renderState.evaluationLayer != nullptr) {
    SDL_DestroyTexture(renderState.evaluationLayer);
    renderState.evaluationLayer = nullptr;
  }

  if (renderState.sdlRenderer != nullptr) {
    SDL_DestroyRenderer(renderState.sdlRenderer);
    renderState.sdlRenderer = nullptr;
  }

  if (renderState.sdlWindow != nullptr) {
    SDL_DestroyWindow(renderState.sdlWindow);
    renderState.sdlWindow = nullptr;
  }

  SDL_Quit();

  return error ? 1 : 0;
}
