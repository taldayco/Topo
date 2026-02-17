#include "game/game.h"
#include "app_state.h"
#include "config.h"
#include "game/camera.h"
#include "game/input.h"
#include "game/render_system.h"
#include "game/sprite.h"
#include "map_gen.h"
#include "imgui_ui.h"

// ---- MenuScene ----

class MenuScene : public Scene {
public:
  MenuScene(Game &game) : game(game) {}

  void init(GpuContext &gpu) override {}

  void handle_event(const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_RETURN) {
      game.switch_scene(SceneID::Game);
    }
  }

  void update(float dt) override {}

  void render(GpuContext &gpu, FrameContext &frame) override {
    gpu_begin_render_pass(gpu, frame);
    // Menu is just a clear screen for now — ImGui can draw on top
  }

  void cleanup() override {}

private:
  Game &game;
};

// ---- GameScene ----

class GameScene : public Scene {
public:
  GameScene(Game &game) : game(game) {}

  void init(GpuContext &gpu) override {
    input.init();
    app_state.heightmap.resize(Config::MAP_WIDTH * Config::MAP_HEIGHT);
    app_state.need_regenerate = true;

    // Create a test entity
    auto entity = game.registry.create();
    game.registry.emplace<PositionComponent>(entity, 50.0f, 50.0f, 0.0f);
    game.registry.emplace<SpriteComponent>(entity);

    camera.follow_x = 50.0f;
    camera.follow_y = 50.0f;
    camera.following = true;
  }

  void handle_event(const SDL_Event &event) override {
    input.handle_event(event);

    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
      game.push_scene(SceneID::Pause);
    }
  }

  void update(float dt) override {
    auto &in = input.state();

    // Move camera with input
    float cam_speed = 200.0f * dt;
    if (in.held[(int)Action::CameraUp])    camera.y -= cam_speed;
    if (in.held[(int)Action::CameraDown])  camera.y += cam_speed;
    if (in.held[(int)Action::CameraLeft])  camera.x -= cam_speed;
    if (in.held[(int)Action::CameraRight]) camera.x += cam_speed;

    if (in.held[(int)Action::ZoomIn])
      camera_system.set_zoom(camera, camera.target_zoom + dt * 2.0f);
    if (in.held[(int)Action::ZoomOut])
      camera_system.set_zoom(camera, camera.target_zoom - dt * 2.0f);

    // Move test entity
    auto view = game.registry.view<PositionComponent>();
    for (auto [entity, pos] : view.each()) {
      float speed = 100.0f * dt;
      if (in.held[(int)Action::MoveUp])    pos.y -= speed;
      if (in.held[(int)Action::MoveDown])  pos.y += speed;
      if (in.held[(int)Action::MoveLeft])  pos.x -= speed;
      if (in.held[(int)Action::MoveRight]) pos.x += speed;

      if (camera.following) {
        camera.follow_x = pos.x;
        camera.follow_y = pos.y;
      }
    }

    camera_system.update(camera, dt);
    camera_system.apply_to_view(camera, app_state.view);

    input.begin_frame();
  }

  void render(GpuContext &gpu, FrameContext &frame) override {
    if (app_state.need_regenerate)
      regenerate_map(app_state, gpu.device, gpu.map_texture);

    if constexpr (Config::use_IMGUI) {
      ui_render(app_state, gpu.map_texture);
      ui_prepare_draw(frame.cmd);
      gpu_begin_render_pass(gpu, frame);
      ui_draw(frame.cmd, frame.render_pass);
    } else {
      if (gpu.map_texture.texture)
        gpu_blit_texture(frame, gpu.map_texture);
    }
  }

  void cleanup() override {
    sprites.cleanup(gpu_device);
  }

  AppState &get_app_state() { return app_state; }

  void set_gpu_device(SDL_GPUDevice *dev) { gpu_device = dev; }

private:
  Game &game;
  AppState app_state;
  InputSystem input;
  CameraState camera;
  CameraSystem camera_system;
  SpriteManager sprites;
  RenderSystem render_sys;
  SDL_GPUDevice *gpu_device = nullptr;
};

// ---- PauseScene ----

class PauseScene : public Scene {
public:
  PauseScene(Game &game) : game(game) {}

  void init(GpuContext &gpu) override {}

  void handle_event(const SDL_Event &event) override {
    if (event.type == SDL_EVENT_KEY_DOWN &&
        event.key.scancode == SDL_SCANCODE_ESCAPE) {
      game.pop_scene();
    }
  }

  void update(float dt) override {}

  void render(GpuContext &gpu, FrameContext &frame) override {
    // Pause overlay renders on top of game scene below it
    // For now, just render the render pass (ImGui can draw pause menu)
  }

  void cleanup() override {}

private:
  Game &game;
};

// ---- Game implementation ----

std::unique_ptr<Scene> Game::create_scene(SceneID id) {
  switch (id) {
  case SceneID::Menu:  return std::make_unique<MenuScene>(*this);
  case SceneID::Game:  return std::make_unique<GameScene>(*this);
  case SceneID::Pause: return std::make_unique<PauseScene>(*this);
  default: return nullptr;
  }
}

void Game::push_scene(SceneID id) {
  auto scene = create_scene(id);
  if (scene) {
    scene->init(*gpu_ref);
    scene_stack.push_back({id, std::move(scene)});
  }
}

void Game::pop_scene() {
  if (!scene_stack.empty()) {
    scene_stack.back().scene->cleanup();
    scene_stack.pop_back();
  }
}

void Game::switch_scene(SceneID id) {
  if (!scene_stack.empty()) {
    scene_stack.back().scene->cleanup();
    scene_stack.pop_back();
  }
  push_scene(id);
}

void Game::init(GpuContext &gpu) {
  gpu_ref = &gpu;
  push_scene(SceneID::Game); // Start directly in game for now
}

void Game::handle_event(const SDL_Event &event) {
  if (!scene_stack.empty())
    scene_stack.back().scene->handle_event(event);
}

void Game::update(float dt) {
  if (!scene_stack.empty())
    scene_stack.back().scene->update(dt);
}

void Game::render(GpuContext &gpu, FrameContext &frame) {
  if (!scene_stack.empty())
    scene_stack.back().scene->render(gpu, frame);
}

void Game::cleanup() {
  while (!scene_stack.empty()) {
    scene_stack.back().scene->cleanup();
    scene_stack.pop_back();
  }
}

Scene *Game::current_scene() {
  return scene_stack.empty() ? nullptr : scene_stack.back().scene.get();
}
