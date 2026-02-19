#include "scenes/menu_scene.h"
#include "topo_game.h"

void MenuScene::handle_event(const SDL_Event &event) {
  if (event.type == SDL_EVENT_KEY_DOWN &&
      event.key.scancode == SDL_SCANCODE_RETURN) {
    game.switch_scene(SceneID::Game);
  }
}

void MenuScene::render(GpuContext &gpu, FrameContext &frame) {
  gpu_begin_render_pass(gpu, frame);
}
