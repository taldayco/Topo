#pragma once
#include "scene/scene.h"
#include "input/input.h"
#include "camera/camera.h"
#include "render/sprite.h"
#include "render/render_system.h"

class TopoGame;

class GameScene : public Scene {
public:
  GameScene(TopoGame &game) : game(game) {}

  void init(GpuContext &gpu) override;
  void handle_event(const SDL_Event &event) override;
  void update(float dt) override;
  void render(GpuContext &gpu, FrameContext &frame) override {}
  void cleanup() override;

  void set_gpu_device(SDL_GPUDevice *dev) { gpu_device = dev; }

private:
  TopoGame &game;
  InputSystem input;
  CameraState camera;
  CameraSystem camera_system;
  SpriteManager sprites;
  RenderSystem render_sys;
  SDL_GPUDevice *gpu_device = nullptr;
};
