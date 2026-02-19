#pragma once
#include "app.h"
#include "app_state.h"
#include "scene/scene_manager.h"
#include <entt/entt.hpp>

enum class SceneID { None, Menu, Game, Pause };

class TopoGame : public Application {
public:
  entt::registry registry;
  AppState app_state;
  SceneManager scenes;

  void push_scene(SceneID id);
  void pop_scene();
  void switch_scene(SceneID id);

  void on_init(GpuContext &gpu) override;
  void on_event(const SDL_Event &event) override;
  void on_update(float dt) override;
  void on_render_tool(GpuContext &gpu, FrameContext &frame) override;
  void on_render_game(GpuContext &gpu, FrameContext &frame) override;
  void on_cleanup() override;

  bool wants_game_window_open() override;
  bool wants_game_window_close() override;

private:
  std::unique_ptr<Scene> create_scene(SceneID id);
  void render_ui(bool game_window_open);
};
