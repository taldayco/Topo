#pragma once
#include "game/scene.h"
#include "gpu.h"
#include <entt/entt.hpp>
#include <memory>
#include <vector>

enum class SceneID { None, Menu, Game, Pause };

class Game {
public:
  entt::registry registry;

  void push_scene(SceneID id);
  void pop_scene();
  void switch_scene(SceneID id);

  void init(GpuContext &gpu);
  void handle_event(const SDL_Event &event);
  void update(float dt);
  void render(GpuContext &gpu, FrameContext &frame);
  void cleanup();

  bool wants_quit() const { return quit_requested; }
  void request_quit() { quit_requested = true; }

  Scene *current_scene();

private:
  std::unique_ptr<Scene> create_scene(SceneID id);

  struct SceneEntry {
    SceneID id;
    std::unique_ptr<Scene> scene;
  };

  std::vector<SceneEntry> scene_stack;
  bool quit_requested = false;
  GpuContext *gpu_ref = nullptr;
};
