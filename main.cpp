#include "config.h"
#include "game/game.h"
#include "gpu.h"
#include "imgui_ui.h"

static constexpr float FIXED_DT = 1.0f / 60.0f;
static constexpr float MAX_FRAME_TIME = 0.25f;

int main() {
  SDL_Log("Application starting...");

  GpuContext gpu = {};
  if (!gpu_init(gpu))
    return 1;

  if constexpr (Config::use_IMGUI)
    ui_init(gpu.window, gpu.device);

  Game game;
  game.init(gpu);

  SDL_Log("Entering main loop");
  bool running = true;
  uint64_t freq = SDL_GetPerformanceFrequency();
  uint64_t prev_time = SDL_GetPerformanceCounter();
  float accumulator = 0.0f;

  while (running) {
    uint64_t current_time = SDL_GetPerformanceCounter();
    float frame_time = (float)(current_time - prev_time) / (float)freq;
    prev_time = current_time;

    if (frame_time > MAX_FRAME_TIME)
      frame_time = MAX_FRAME_TIME;

    accumulator += frame_time;

    // Process events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if constexpr (Config::use_IMGUI)
        ui_process_event(event);
      if (event.type == SDL_EVENT_QUIT)
        running = false;
      game.handle_event(event);
    }

    if (game.wants_quit())
      running = false;

    // Fixed timestep update
    while (accumulator >= FIXED_DT) {
      game.update(FIXED_DT);
      accumulator -= FIXED_DT;
    }

    // Render
    FrameContext frame;
    if (gpu_acquire_frame(gpu, frame)) {
      game.render(gpu, frame);
      gpu_end_frame(frame);
    }
  }

  game.cleanup();

  if constexpr (Config::use_IMGUI)
    ui_shutdown();
  gpu_cleanup(gpu);
  return 0;
}
