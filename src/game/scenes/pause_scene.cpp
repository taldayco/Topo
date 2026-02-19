#include "scenes/pause_scene.h"
#include "topo_game.h"

void PauseScene::handle_event(const SDL_Event &event) {
  if (event.type == SDL_EVENT_KEY_DOWN &&
      event.key.scancode == SDL_SCANCODE_ESCAPE) {
    game.pop_scene();
  }
}
