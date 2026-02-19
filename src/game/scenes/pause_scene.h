#pragma once
#include "scene/scene.h"

class TopoGame;

class PauseScene : public Scene {
public:
  PauseScene(TopoGame &game) : game(game) {}

  void init(GpuContext &gpu) override {}
  void handle_event(const SDL_Event &event) override;
  void update(float dt) override {}
  void render(GpuContext &gpu, FrameContext &frame) override {}
  void cleanup() override {}

private:
  TopoGame &game;
};
