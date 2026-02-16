#include "app_state.h"
#include "config.h"
#include "gpu.h"
#include "map_gen.h"
#include "imgui_ui.h"

int main() {
  SDL_Log("Application starting...");

  GpuContext gpu = {};
  if (!gpu_init(gpu))
    return 1;

  if constexpr (Config::use_IMGUI)
    ui_init(gpu.window, gpu.device);

  AppState state = {};
  state.heightmap.resize(Config::MAP_WIDTH * Config::MAP_HEIGHT);

  SDL_Log("Entering main loop");
  bool running = true;
  while (running) {
    if (state.need_regenerate)
      regenerate_map(state, gpu.device, gpu.map_texture);

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if constexpr (Config::use_IMGUI)
        ui_process_event(event);
      if (event.type == SDL_EVENT_QUIT)
        running = false;
    }

    if constexpr (Config::use_IMGUI) {
      ui_render(state, gpu.map_texture);

      FrameContext frame;
      if (gpu_acquire_frame(gpu, frame)) {
        ui_prepare_draw(frame.cmd);
        gpu_begin_render_pass(gpu, frame);
        ui_draw(frame.cmd, frame.render_pass);
        gpu_end_frame(frame);
      }
    } else {
      FrameContext frame;
      if (gpu_acquire_frame(gpu, frame)) {
        if (gpu.map_texture.texture)
          gpu_blit_texture(frame, gpu.map_texture);
        gpu_end_frame(frame);
      }
    }
  }

  if constexpr (Config::use_IMGUI)
    ui_shutdown();
  gpu_cleanup(gpu);
  return 0;
}
