#pragma once
#include "app_state.h"

struct CameraState {
  float x = 0.0f;
  float y = 0.0f;
  float zoom = 1.0f;
  float target_zoom = 1.0f;

  float follow_x = 0.0f;
  float follow_y = 0.0f;
  bool following = false;

  float shake_intensity = 0.0f;
  float shake_duration = 0.0f;
  float shake_timer = 0.0f;

  // Map bounds for clamping
  float min_x = 0.0f, max_x = 1024.0f;
  float min_y = 0.0f, max_y = 1024.0f;
  float min_zoom = 0.25f, max_zoom = 4.0f;

  float follow_speed = 5.0f;
  float zoom_speed = 5.0f;
};

class CameraSystem {
public:
  void update(CameraState &cam, float dt);
  void follow(CameraState &cam, float target_x, float target_y);
  void stop_follow(CameraState &cam);
  void shake(CameraState &cam, float intensity, float duration);
  void set_zoom(CameraState &cam, float zoom);

  void apply_to_view(const CameraState &cam, ViewState &view);
};
