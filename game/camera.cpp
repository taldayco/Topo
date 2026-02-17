#include "game/camera.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

static float lerp(float a, float b, float t) {
  return a + (b - a) * t;
}

void CameraSystem::update(CameraState &cam, float dt) {
  // Smooth follow
  if (cam.following) {
    float t = 1.0f - std::exp(-cam.follow_speed * dt);
    cam.x = lerp(cam.x, cam.follow_x, t);
    cam.y = lerp(cam.y, cam.follow_y, t);
  }

  // Smooth zoom
  float zoom_t = 1.0f - std::exp(-cam.zoom_speed * dt);
  cam.zoom = lerp(cam.zoom, cam.target_zoom, zoom_t);

  // Clamp zoom
  cam.zoom = std::clamp(cam.zoom, cam.min_zoom, cam.max_zoom);
  cam.target_zoom = std::clamp(cam.target_zoom, cam.min_zoom, cam.max_zoom);

  // Clamp position
  cam.x = std::clamp(cam.x, cam.min_x, cam.max_x);
  cam.y = std::clamp(cam.y, cam.min_y, cam.max_y);

  // Screen shake
  if (cam.shake_timer > 0.0f) {
    cam.shake_timer -= dt;
    if (cam.shake_timer < 0.0f)
      cam.shake_timer = 0.0f;
  }
}

void CameraSystem::follow(CameraState &cam, float target_x, float target_y) {
  cam.follow_x = target_x;
  cam.follow_y = target_y;
  cam.following = true;
}

void CameraSystem::stop_follow(CameraState &cam) {
  cam.following = false;
}

void CameraSystem::shake(CameraState &cam, float intensity, float duration) {
  cam.shake_intensity = intensity;
  cam.shake_duration = duration;
  cam.shake_timer = duration;
}

void CameraSystem::set_zoom(CameraState &cam, float zoom) {
  cam.target_zoom = std::clamp(zoom, cam.min_zoom, cam.max_zoom);
}

void CameraSystem::apply_to_view(const CameraState &cam, ViewState &view) {
  view.zoom = cam.zoom;

  // Convert world position to normalized pan (0-1 range)
  float range_x = cam.max_x - cam.min_x;
  float range_y = cam.max_y - cam.min_y;
  view.pan_x = (range_x > 0.0f) ? (cam.x - cam.min_x) / range_x : 0.5f;
  view.pan_y = (range_y > 0.0f) ? (cam.y - cam.min_y) / range_y : 0.5f;

  // Apply shake offset
  if (cam.shake_timer > 0.0f) {
    float fade = cam.shake_timer / cam.shake_duration;
    float shake_x = ((float)(rand() % 1000) / 500.0f - 1.0f) * cam.shake_intensity * fade;
    float shake_y = ((float)(rand() % 1000) / 500.0f - 1.0f) * cam.shake_intensity * fade;
    view.pan_x += shake_x / (range_x > 0.0f ? range_x : 1.0f);
    view.pan_y += shake_y / (range_y > 0.0f ? range_y : 1.0f);
  }
}
