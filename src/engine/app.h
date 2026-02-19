#pragma once
#include "gpu/gpu.h"
#include "ui/imgui_ui.h"
#include <SDL3/SDL.h>

class Application {
public:
  virtual ~Application() = default;

  int run();

  // Override points
  virtual void on_init(GpuContext &gpu) = 0;
  virtual void on_event(const SDL_Event &event) = 0;
  virtual void on_update(float dt) = 0;
  virtual void on_render_tool(GpuContext &gpu, FrameContext &frame) = 0;
  virtual void on_render_game(GpuContext &gpu, FrameContext &frame) = 0;
  virtual void on_cleanup() = 0;

  // Game window management
  virtual bool wants_game_window_open() { return false; }
  virtual bool wants_game_window_close() { return false; }

  GpuContext &gpu() { return gpu_ctx; }
  void request_quit() { running = false; }

protected:
  GpuContext gpu_ctx;
  bool running = true;
};
