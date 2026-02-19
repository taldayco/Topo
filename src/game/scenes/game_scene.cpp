#include "scenes/game_scene.h"
#include "topo_game.h"
#include "config.h"

void GameScene::init(GpuContext &gpu) {
  input.init();
  game.app_state.heightmap.resize(Config::MAP_WIDTH * Config::MAP_HEIGHT);
  game.app_state.need_regenerate = true;

  auto entity = game.registry.create();
  game.registry.emplace<PositionComponent>(entity, 50.0f, 50.0f, 0.0f);
  game.registry.emplace<SpriteComponent>(entity);

  camera.max_x = Config::MAP_WIDTH;
  camera.max_y = Config::MAP_HEIGHT;
  camera.x = Config::MAP_WIDTH / 2.0f;
  camera.y = Config::MAP_HEIGHT / 2.0f;
  camera.follow_x = Config::MAP_WIDTH / 2.0f;
  camera.follow_y = Config::MAP_HEIGHT / 2.0f;
  camera.following = true;
}

void GameScene::handle_event(const SDL_Event &event) {
  input.handle_event(event);

  if (event.type == SDL_EVENT_KEY_DOWN &&
      event.key.scancode == SDL_SCANCODE_ESCAPE) {
    game.push_scene(SceneID::Pause);
  }
}

void GameScene::update(float dt) {
  auto &in = input.state();
  auto &app_state = game.app_state;

  float cam_speed = 200.0f * dt;
  bool cam_moved = in.held[(int)Action::CameraUp] ||
                   in.held[(int)Action::CameraDown] ||
                   in.held[(int)Action::CameraLeft] ||
                   in.held[(int)Action::CameraRight];
  if (cam_moved)
    camera.following = false;
  if (in.held[(int)Action::CameraUp])    camera.y -= cam_speed;
  if (in.held[(int)Action::CameraDown])  camera.y += cam_speed;
  if (in.held[(int)Action::CameraLeft])  camera.x -= cam_speed;
  if (in.held[(int)Action::CameraRight]) camera.x += cam_speed;

  if (in.held[(int)Action::ZoomIn])
    camera_system.set_zoom(camera, camera.target_zoom + dt * 2.0f);
  if (in.held[(int)Action::ZoomOut])
    camera_system.set_zoom(camera, camera.target_zoom - dt * 2.0f);

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

void GameScene::cleanup() {
  sprites.cleanup(gpu_device);
}
