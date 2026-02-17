#pragma once
#include "game/camera.h"
#include "game/sprite.h"
#include "isometric.h"
#include <entt/entt.hpp>
#include <vector>

struct RenderableEntity {
  float iso_x, iso_y;
  float depth;
  entt::entity entity;
};

class RenderSystem {
public:
  void render_entities(entt::registry &registry,
                       std::vector<uint32_t> &pixels,
                       int view_width, int view_height,
                       float offset_x, float offset_y,
                       const IsometricParams &params,
                       const CameraState &camera,
                       const SpriteManager &sprites);

private:
  void blit_sprite(std::vector<uint32_t> &pixels, int view_width, int view_height,
                   const SpriteSheet &sheet, const SpriteFrame &frame,
                   int dst_x, int dst_y, bool flip_x);

  std::vector<RenderableEntity> sorted_entities;
};
