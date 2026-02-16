#include "isometric.h"
#include "basalt.h"
#include "config.h"
#include "terrain_generator.h"
#include "util.h"
#include "water.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>

void world_to_iso(float x, float y, float z, float &out_x, float &out_y,
                  const IsometricParams &params) {
  out_x = (x - y) * params.tile_width;
  out_y = (x + y) * params.tile_height - z * params.height_scale;
}

static void draw_line_iso(std::vector<uint32_t> &pixels, int width, int height,
                          float x0, float y0, float x1, float y1,
                          uint32_t color) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = std::hypot(dx, dy);
  if (len < 1.0f)
    return;
  float step_x = dx / len;
  float step_y = dy / len;
  uint8_t alpha = (color >> 24) & 0xFF;
  uint8_t r = (color >> 16) & 0xFF;
  uint8_t g = (color >> 8) & 0xFF;
  uint8_t b = color & 0xFF;
  for (int i = 0; i <= (int)len; ++i) {
    int x = (int)(x0 + step_x * i);
    int y = (int)(y0 + step_y * i);
    if (x >= 0 && x < width && y >= 0 && y < height) {
      uint32_t dst = pixels[y * width + x];
      uint8_t dst_r = (dst >> 16) & 0xFF;
      uint8_t dst_g = (dst >> 8) & 0xFF;
      uint8_t dst_b = dst & 0xFF;
      float blend = alpha / 255.0f;
      uint8_t out_r = r * blend + dst_r * (1.0f - blend);
      uint8_t out_g = g * blend + dst_g * (1.0f - blend);
      uint8_t out_b = b * blend + dst_b * (1.0f - blend);
      pixels[y * width + x] = 0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
    }
  }
}

IsometricView create_isometric_heightmap(
    std::span<const float> heightmap, std::span<const int> band_map,
    std::span<const Line> contour_lines, int map_width, int map_height,
    const IsometricParams &params, const Palette &palette,
    float contour_opacity, float padding, float offset_x_adjust,
    float offset_y_adjust) {

  auto terrain =
      TerrainGenerator::generate(heightmap, band_map, map_width, map_height);

  if (terrain.columns.empty()) {
    SDL_Log("Warning: No columns generated, creating fallback view");
    IsometricView view;
    view.width = map_width * 4;
    view.height = map_height * 4;
    view.pixels.resize(view.width * view.height, Config::BACKGROUND_COLOR);
    return view;
  }

  float min_x = 1e9f, max_x = -1e9f;
  float min_y = 1e9f, max_y = -1e9f;

  float test_heights[] = {0.0f, 0.5f, 1.0f};
  int test_coords[][2] = {
      {0, 0}, {map_width, 0}, {map_width, map_height}, {0, map_height}};

  for (auto h : test_heights) {
    for (auto [x, y] : test_coords) {
      float iso_x, iso_y;
      world_to_iso(x, y, h, iso_x, iso_y, params);
      min_x = std::min(min_x, iso_x);
      max_x = std::max(max_x, iso_x);
      min_y = std::min(min_y, iso_y);
      max_y = std::max(max_y, iso_y);
    }
  }

  int view_width = (int)(max_x - min_x) + (int)(padding * 2);
  int view_height = (int)(max_y - min_y) + (int)(padding * 2);

  IsometricView view;
  view.width = view_width;
  view.height = view_height;
  view.pixels.resize(view_width * view_height, Config::BACKGROUND_COLOR);

  float offset_x = -min_x + padding + offset_x_adjust;
  float offset_y = -min_y + padding + offset_y_adjust;

  // Debug: draw unused regions with translucent per-region colors (before
  // basalt/water so they occlude)
  if (Config::enable_debug_overlay) {
    constexpr float ALPHA = 0.35f;
    for (size_t ri = 0; ri < terrain.unused_regions.size(); ++ri) {
      uint32_t h = hash1d((int)ri);
      uint8_t cr = 80 + (h & 0x7F);
      uint8_t cg = 80 + ((h >> 8) & 0x7F);
      uint8_t cb = 80 + ((h >> 16) & 0x7F);

      for (int idx : terrain.unused_regions[ri].pixels) {
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
              uint32_t dst = view.pixels[py * view_width + px];
              uint8_t dr = (dst >> 16) & 0xFF;
              uint8_t dg = (dst >> 8) & 0xFF;
              uint8_t db = dst & 0xFF;
              uint8_t out_r = (uint8_t)(cr * ALPHA + dr * (1.0f - ALPHA));
              uint8_t out_g = (uint8_t)(cg * ALPHA + dg * (1.0f - ALPHA));
              uint8_t out_b = (uint8_t)(cb * ALPHA + db * (1.0f - ALPHA));
              view.pixels[py * view_width + px] =
                  0xFF000000 | (out_r << 16) | (out_g << 8) | out_b;
            }
          }
        }
      }
    }
  }

  render_basalt_columns(view.pixels, view_width, view_height, terrain.columns,
                        Config::HEX_SIZE, offset_x, offset_y, params, palette);

  float time = SDL_GetTicks() / 1000.0f;
  render_water(view.pixels, view_width, view_height, terrain.water_bodies,
               offset_x, offset_y, params, time);

  uint32_t line_color = ((uint8_t)(contour_opacity * 255) << 24) | 0x00DDDDDD;

  for (const auto &line : contour_lines) {
    float iso_x0, iso_y0, iso_x1, iso_y1;
    world_to_iso(line.x1, line.y1, line.elevation, iso_x0, iso_y0, params);
    world_to_iso(line.x2, line.y2, line.elevation, iso_x1, iso_y1, params);
    draw_line_iso(view.pixels, view_width, view_height, iso_x0 + offset_x,
                  iso_y0 + offset_y, iso_x1 + offset_x, iso_y1 + offset_y,
                  line_color);
  }

  return view;
}
