#include "render/render_system.h"
#include <algorithm>
#include <cstdlib>

static uint32_t alpha_blend_rs(uint32_t src, uint32_t dst, float alpha) {
  uint8_t sr = (src >> 16) & 0xFF;
  uint8_t sg = (src >> 8) & 0xFF;
  uint8_t sb = src & 0xFF;
  uint8_t dr = (dst >> 16) & 0xFF;
  uint8_t dg = (dst >> 8) & 0xFF;
  uint8_t db = dst & 0xFF;
  uint8_t r = (uint8_t)(sr * alpha + dr * (1.0f - alpha));
  uint8_t g = (uint8_t)(sg * alpha + dg * (1.0f - alpha));
  uint8_t b = (uint8_t)(sb * alpha + db * (1.0f - alpha));
  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

void RenderSystem::render_entities(entt::registry &registry,
                                   std::vector<uint32_t> &pixels,
                                   int view_width, int view_height,
                                   float offset_x, float offset_y,
                                   const IsometricParams &params,
                                   const CameraState &camera,
                                   const SpriteManager &sprites,
                                   WorldToIsoFn world_to_iso_fn) {
  sorted_entities.clear();

  auto view = registry.view<PositionComponent, SpriteComponent>();
  for (auto [entity, pos, sprite] : view.each()) {
    float iso_x, iso_y;
    world_to_iso_fn(pos.x, pos.y, pos.z, iso_x, iso_y, params);
    iso_x += offset_x;
    iso_y += offset_y;

    float depth = pos.x + pos.y;

    sorted_entities.push_back({iso_x, iso_y, depth, entity});
  }

  std::sort(sorted_entities.begin(), sorted_entities.end(),
            [](const RenderableEntity &a, const RenderableEntity &b) {
              return a.depth < b.depth;
            });

  for (auto &re : sorted_entities) {
    auto &sprite = registry.get<SpriteComponent>(re.entity);
    const SpriteSheet *sheet = sprites.get_sheet(sprite.sheet_id);
    if (!sheet || sheet->frames.empty())
      continue;

    int frame_idx = sprite.current_frame % (int)sheet->frames.size();
    const SpriteFrame &frame = sheet->frames[frame_idx];

    int dst_x = (int)re.iso_x - frame.w / 2;
    int dst_y = (int)re.iso_y - frame.h;

    blit_sprite(pixels, view_width, view_height, *sheet, frame,
                dst_x, dst_y, sprite.flip_x);
  }
}

void RenderSystem::blit_sprite(std::vector<uint32_t> &pixels,
                               int view_width, int view_height,
                               const SpriteSheet &sheet,
                               const SpriteFrame &frame,
                               int dst_x, int dst_y, bool flip_x) {
  int cx = dst_x + frame.w / 2;
  int cy = dst_y + frame.h / 2;
  int radius = frame.w > 0 ? frame.w / 3 : 8;

  uint32_t color = 0xFFFF4444;

  for (int dy = -radius; dy <= radius; ++dy) {
    for (int dx = -radius; dx <= radius; ++dx) {
      if (std::abs(dx) + std::abs(dy) <= radius) {
        int px = cx + dx;
        int py = cy + dy;
        if (px >= 0 && px < view_width && py >= 0 && py < view_height) {
          pixels[py * view_width + px] =
              alpha_blend_rs(color, pixels[py * view_width + px], 0.8f);
        }
      }
    }
  }
}
