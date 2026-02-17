#include "game/input.h"
#include <cmath>

void InputSystem::init() {
  // Default key bindings
  bind(SDL_SCANCODE_W, Action::MoveUp);
  bind(SDL_SCANCODE_S, Action::MoveDown);
  bind(SDL_SCANCODE_A, Action::MoveLeft);
  bind(SDL_SCANCODE_D, Action::MoveRight);
  bind(SDL_SCANCODE_E, Action::Interact);
  bind(SDL_SCANCODE_Q, Action::Cancel);
  bind(SDL_SCANCODE_ESCAPE, Action::Pause);
  bind(SDL_SCANCODE_EQUALS, Action::ZoomIn);
  bind(SDL_SCANCODE_MINUS, Action::ZoomOut);
  bind(SDL_SCANCODE_UP, Action::CameraUp);
  bind(SDL_SCANCODE_DOWN, Action::CameraDown);
  bind(SDL_SCANCODE_LEFT, Action::CameraLeft);
  bind(SDL_SCANCODE_RIGHT, Action::CameraRight);
}

void InputSystem::begin_frame() {
  for (int i = 0; i < (int)Action::Count; ++i) {
    current.pressed[i] = false;
    current.released[i] = false;
  }
  current.mouse_left_pressed = false;
  current.mouse_right_pressed = false;
}

void InputSystem::handle_event(const SDL_Event &event) {
  switch (event.type) {
  case SDL_EVENT_KEY_DOWN: {
    if (event.key.repeat)
      break;
    auto it = key_bindings.find(event.key.scancode);
    if (it != key_bindings.end()) {
      int idx = (int)it->second;
      current.pressed[idx] = true;
      current.held[idx] = true;
    }
    break;
  }
  case SDL_EVENT_KEY_UP: {
    auto it = key_bindings.find(event.key.scancode);
    if (it != key_bindings.end()) {
      int idx = (int)it->second;
      current.released[idx] = true;
      current.held[idx] = false;
    }
    break;
  }
  case SDL_EVENT_MOUSE_MOTION:
    current.mouse_x = event.motion.x;
    current.mouse_y = event.motion.y;
    break;
  case SDL_EVENT_MOUSE_BUTTON_DOWN:
    if (event.button.button == SDL_BUTTON_LEFT) {
      current.mouse_left_pressed = true;
      current.mouse_left_held = true;
    } else if (event.button.button == SDL_BUTTON_RIGHT) {
      current.mouse_right_pressed = true;
      current.mouse_right_held = true;
    }
    break;
  case SDL_EVENT_MOUSE_BUTTON_UP:
    if (event.button.button == SDL_BUTTON_LEFT)
      current.mouse_left_held = false;
    else if (event.button.button == SDL_BUTTON_RIGHT)
      current.mouse_right_held = false;
    break;
  }
}

void InputSystem::update_mouse_world(float screen_x, float screen_y,
                                     float view_offset_x, float view_offset_y,
                                     const IsometricParams &params) {
  // Convert screen coordinates to isometric world coordinates
  // Inverse of world_to_iso: iso_x = (x - y) * tile_width
  //                          iso_y = (x + y) * tile_height - z * height_scale
  // Assuming z=0 for mouse picking on the ground plane:
  //   iso_x = (x - y) * tw
  //   iso_y = (x + y) * th
  // Solving: x = (iso_x/tw + iso_y/th) / 2
  //          y = (iso_y/th - iso_x/tw) / 2

  float iso_x = screen_x - view_offset_x;
  float iso_y = screen_y - view_offset_y;

  float tw = params.tile_width;
  float th = params.tile_height;

  current.mouse_world_x = (iso_x / tw + iso_y / th) * 0.5f;
  current.mouse_world_y = (iso_y / th - iso_x / tw) * 0.5f;
}

void InputSystem::bind(SDL_Scancode key, Action action) {
  key_bindings[key] = action;
}
