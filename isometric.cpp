#include "isometric.h"
#include <algorithm>
#include <cmath>

void world_to_iso(float x, float y, float z, float &out_x, float &out_y,
                  const IsometricParams &params) {
  // Standard isometric projection
  out_x = (x - y) * params.tile_width;
  out_y = (x + y) * params.tile_height - z * params.height_scale;
}

static void draw_line_iso(std::vector<uint32_t> &pixels, int width, int height,
                          float x0, float y0, float x1, float y1,
                          uint32_t color) {
  float dx = x1 - x0;
  float dy = y1 - y0;
  float len = std::max(std::abs(dx), std::abs(dy));

  if (len < 1.0f)
    return;

  float step_x = dx / len;
  float step_y = dy / len;

  for (int i = 0; i <= (int)len; ++i) {
    int x = (int)(x0 + step_x * i);
    int y = (int)(y0 + step_y * i);

    if (x >= 0 && x < width && y >= 0 && y < height) {
      pixels[y * width + x] = color;
    }
  }
}

IsometricView create_isometric_heightmap(std::span<const float> heightmap,
                                         std::span<const Line> contour_lines,
                                         int map_width, int map_height,
                                         const IsometricParams &params,
                                         const Palette &palette) {

  // Calculate isometric view dimensions
  float min_x = 1e9f, max_x = -1e9f;
  float min_y = 1e9f, max_y = -1e9f;

  // Find bounds by projecting all corners
  for (int y = 0; y <= map_height; ++y) {
    for (int x = 0; x <= map_width; ++x) {
      float z = 0;
      if (x < map_width && y < map_height) {
        z = heightmap[y * map_width + x];
      }
      float iso_x, iso_y;
      world_to_iso(x, y, z, iso_x, iso_y, params);
      min_x = std::min(min_x, iso_x);
      max_x = std::max(max_x, iso_x);
      min_y = std::min(min_y, iso_y);
      max_y = std::max(max_y, iso_y);
    }
  }

  int view_width = (int)(max_x - min_x) + 20; // Padding
  int view_height = (int)(max_y - min_y) + 20;

  IsometricView view;
  view.width = view_width;
  view.height = view_height;
  view.pixels.resize(view_width * view_height, 0xFF2D2D30); // Background color

  float offset_x = -min_x + 10;
  float offset_y = -min_y + 10;

  // Render heightmap pixels back-to-front

  for (int y = map_height - 1; y >= 0; --y) {
    for (int x = map_width - 1; x >= 0; --x) {
      float z = heightmap[y * map_width + x];
      uint32_t color = organic_color(z, x, y, palette);

      // Project quad corners
      float corners_x[4], corners_y[4];
      world_to_iso(x, y, z, corners_x[0], corners_y[0], params);
      world_to_iso(x + 1, y, z, corners_x[1], corners_y[1], params);
      world_to_iso(x + 1, y + 1, z, corners_x[2], corners_y[2], params);
      world_to_iso(x, y + 1, z, corners_x[3], corners_y[3], params);

      // Draw filled diamond (simple scanline fill)
      float center_x = (corners_x[0] + corners_x[2]) / 2 + offset_x;
      float center_y = (corners_y[0] + corners_y[2]) / 2 + offset_y;

      for (int i = 0; i < 4; ++i) {
        corners_x[i] += offset_x;
        corners_y[i] += offset_y;
      }

      // Simple diamond rasterization
      for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
          int px = (int)center_x + dx;
          int py = (int)center_y + dy;
          if (px >= 0 && px < view_width && py >= 0 && py < view_height) {
            view.pixels[py * view_width + px] = color;
          }
        }
      }
    }
  }

  // Render contour lines
  uint32_t line_color = palette.colors[5];
  for (const auto &line : contour_lines) {
    float z0 = heightmap[(int)line.y1 * map_width + (int)line.x1];
    float z1 = heightmap[(int)line.y2 * map_width + (int)line.x2];

    float iso_x0, iso_y0, iso_x1, iso_y1;
    world_to_iso(line.x1, line.y1, z0, iso_x0, iso_y0, params);
    world_to_iso(line.x2, line.y2, z1, iso_x1, iso_y1, params);

    draw_line_iso(view.pixels, view_width, view_height, iso_x0 + offset_x,
                  iso_y0 + offset_y, iso_x1 + offset_x, iso_y1 + offset_y,
                  line_color);
  }

  return view;
}
