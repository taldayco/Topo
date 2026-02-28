#pragma once
#include "app.h"
#include "game_state.h"
#include "terrain/noise_layers.h"
#include "terrain/noise_cache.h"
#include "terrain/noise_composer.h"
#include "terrain/map_data.h"
#include "terrain/terrain_renderer.h"
#include "terrain/terrain_mesh.h"
#include "input/input.h"
#include "camera/camera.h"
#include "render/background.h"
#include <glm/glm.hpp>
#include <vector>

class TopoGame : public Application {
public:
  TerrainRenderer    terrain_renderer;
  BackgroundRenderer background_renderer;
  InputSystem        input;
  CameraState        camera;
  CameraSystem       camera_system;
  std::vector<GpuPointLight> point_lights;

  void on_init(GpuContext &gpu, flecs::world &ecs) override;
  void on_event(const SDL_Event &event, flecs::world &ecs) override;
  void on_render_tool(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) override;
  void on_render_game(GpuContext &gpu, FrameContext &frame, flecs::world &ecs) override;
  void on_cleanup(flecs::world &ecs) override;

  bool wants_game_window_open(flecs::world &ecs) override;
  bool wants_game_window_close(flecs::world &ecs) override;

private:
  void render_ui(flecs::world &ecs, bool game_window_open);
  int save_status_timer = 0;
};
