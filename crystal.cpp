#include "crystal.h"

void render_crystal_debug_overlay(std::vector<uint32_t> &pixels, int view_width,
    int view_height, const std::vector<UnusedRegion> &regions,
    std::span<const float> heightmap, int map_width,
    float offset_x, float offset_y, const IsometricParams &params) {
  constexpr float ALPHA = 0.35f;
  constexpr uint8_t cr = 0, cg = 140, cb = 0;

  for (const auto &region : regions) {
    if (region.type != RegionType::Crystal)
      continue;

    for (int idx : region.pixels) {
      int wx = idx % map_width;
      int wy = idx / map_width;
      float z = heightmap[idx];

      float iso_x, iso_y;
      world_to_iso((float)wx, (float)wy, z, iso_x, iso_y, params);
      int sx = (int)(iso_x + offset_x);
      int sy = (int)(iso_y + offset_y);

      constexpr int R = 2;
      for (int dy = -R; dy <= R; ++dy) {
        for (int dx = -R; dx <= R; ++dx) {
          if (dx * dx + dy * dy > R * R)
            continue;
          int px = sx + dx;
          int py = sy + dy;
          if (px >= 0 && px < view_width && py >= 0 && py < view_height) {
            uint32_t dst = pixels[py * view_width + px];
            uint8_t dr = (dst >> 16) & 0xFF;
            uint8_t dg = (dst >> 8) & 0xFF;
            uint8_t db = dst & 0xFF;
            uint8_t out_r = (uint8_t)(cr * ALPHA + dr * (1.0f - ALPHA));
            uint8_t out_g = (uint8_t)(cg * ALPHA + dg * (1.0f - ALPHA));
            uint8_t out_b = (uint8_t)(cb * ALPHA + db * (1.0f - ALPHA));
            pixels[py * view_width + px] =
                0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
          }
        }
      }
    }
  }
}
