#include "isometric.h"
#include "basalt.h"
#include "plateau.h"
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
    std::span<const float> heightmap, std::span<const Line> contour_lines,
    int map_width, int map_height, const IsometricParams &params,
    const Palette &palette, float contour_opacity, float padding,
    float offset_x_adjust, float offset_y_adjust) {

  const float HEX_SIZE = 8.0f;

  std::vector<Plateau> plateaus =
      detect_plateaus(heightmap, map_width, map_height);

  std::vector<int> plateaus_with_columns;
  std::vector<HexColumn> columns = generate_basalt_columns(
      heightmap, map_width, map_height, HEX_SIZE, plateaus_with_columns);

  std::vector<WaterBody> water_bodies = identify_water_bodies(
      heightmap, map_width, map_height, plateaus, plateaus_with_columns);

  if (columns.empty()) {
    SDL_Log("Warning: No columns generated, creating fallback view");

    IsometricView view;
    view.width = map_width * 4;
    view.height = map_height * 4;
    view.pixels.resize(view.width * view.height, 0xFF2D2D30);
    return view;
  }

  float min_x = 1e9f, max_x = -1e9f;
  float min_y = 1e9f, max_y = -1e9f;

  for (const auto &col : columns) {
    Vec2 corners[6];
    get_hex_corners(col.q, col.r, HEX_SIZE, corners);

    for (int i = 0; i < 6; ++i) {
      float iso_x, iso_y;
      world_to_iso(corners[i].x, corners[i].y, col.height, iso_x, iso_y,
                   params);
      min_x = std::min(min_x, iso_x);
      max_x = std::max(max_x, iso_x);
      min_y = std::min(min_y, iso_y);
      max_y = std::max(max_y, iso_y);

      world_to_iso(corners[i].x, corners[i].y, 0.0f, iso_x, iso_y, params);
      min_y = std::min(min_y, iso_y);
      max_y = std::max(max_y, iso_y);
    }
  }

  int view_width = (int)(max_x - min_x) + (int)(padding * 2);
  int view_height = (int)(max_y - min_y) + (int)(padding * 2);

  IsometricView view;
  view.width = view_width;
  view.height = view_height;
  view.pixels.resize(view_width * view_height, 0xFF2D2D30);

  float offset_x = -min_x + padding + offset_x_adjust;
  float offset_y = -min_y + padding + offset_y_adjust;

  render_basalt_columns(view.pixels, view_width, view_height, columns, HEX_SIZE,
                        offset_x, offset_y, params, palette);

  float time = SDL_GetTicks() / 1000.0f;
  render_water(view.pixels, view_width, view_height, water_bodies, offset_x,
               offset_y, params, time);

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
