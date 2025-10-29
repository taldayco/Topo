#include "water.h"
#include "basalt.h"
#include "isometric.h"
#include "plateau.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <cmath>
#include <unordered_set>

static uint32_t hash_plateau(int idx) { return (idx * 374761393) ^ 668265263; }

void generate_water_mesh(WaterBody &water, float grid_spacing) {
  float width = water.max_x - water.min_x;
  float height = water.max_y - water.min_y;

  int grid_w = (int)(width / grid_spacing) + 2;
  int grid_h = (int)(height / grid_spacing) + 2;

  water.mesh.grid_width = grid_w;
  water.mesh.grid_height = grid_h;
  water.mesh.vertices.clear();
  water.mesh.vertices.reserve(grid_w * grid_h);

  for (int gy = 0; gy < grid_h; ++gy) {
    for (int gx = 0; gx < grid_w; ++gx) {
      WaterVertex vert;
      vert.x = water.min_x + gx * grid_spacing;
      vert.y = water.min_y + gy * grid_spacing;
      vert.base_z = water.height;

      water.mesh.vertices.push_back(vert);
    }
  }

  SDL_Log("Generated water mesh: %dx%d grid (%zu vertices) for water body",
          grid_w, grid_h, water.mesh.vertices.size());
}

std::vector<WaterBody>
identify_water_bodies(std::span<const float> heightmap, int width, int height,
                      const std::vector<Plateau> &plateaus,
                      const std::vector<int> &plateaus_with_columns) {

  std::unordered_set<int> plateau_set(plateaus_with_columns.begin(),
                                      plateaus_with_columns.end());

  SDL_Log("Plateaus: %zu total, %zu with columns", plateaus.size(),
          plateau_set.size());

  std::vector<WaterBody> water_bodies;

  int checked = 0;
  for (size_t p = 0; p < plateaus.size(); ++p) {
    if (plateau_set.count(p))
      continue;

    checked++;
    const auto &plateau = plateaus[p];

    SDL_Log("Checking plateau %zu for water: size=%zu", p,
            plateau.pixels.size());

    // Calculate bounding box
    float min_x = 1e9f, max_x = -1e9f;
    float min_y = 1e9f, max_y = -1e9f;

    for (int idx : plateau.pixels) {
      int x = idx % width;
      int y = idx / width;

      min_x = std::min(min_x, (float)x);
      max_x = std::max(max_x, (float)x);
      min_y = std::min(min_y, (float)y);
      max_y = std::max(max_y, (float)y);
    }

    float w = max_x - min_x + 1;
    float h = max_y - min_y + 1;
    float aspect_ratio = std::max(w, h) / std::min(w, h);

    SDL_Log("  Dimensions: %.1fx%.1f, ratio=%.2f", w, h, aspect_ratio);

    // LOWER CRITERIA: ratio > 1.5 and size > 200
    bool is_thin_long = aspect_ratio > 1.5f;
    bool is_large_enough = plateau.pixels.size() > 200;

    SDL_Log("  is_thin_long=%d, is_large_enough=%d", is_thin_long,
            is_large_enough);

    if (is_thin_long && is_large_enough) {
      WaterBody water;
      water.plateau_index = p;
      water.height = plateau.height;
      water.min_x = min_x;
      water.max_x = max_x;
      water.min_y = min_y;
      water.max_y = max_y;
      water.aspect_ratio = aspect_ratio;
      water.pixels = plateau.pixels;
      water.time_offset = (hash_plateau(p) % 1000) / 1000.0f * 6.28f; // 0-2π

      generate_water_mesh(water, 10.0f);

      water_bodies.push_back(water);

      SDL_Log("Water body %zu: plateau %d, size=%zu, "
              "bounds=(%.1f,%.1f)-(%.1f,%.1f), ratio=%.2f",
              water_bodies.size() - 1, p, plateau.pixels.size(), min_x, min_y,
              max_x, max_y, aspect_ratio);
    }
  }

  SDL_Log("Checked %d plateaus without columns", checked);
  SDL_Log("Identified %zu water bodies from %zu plateaus", water_bodies.size(),
          plateaus.size());

  return water_bodies;
}
float get_water_height(float x, float y, float base_z, float time,
                       float time_offset) {
  float t = time + time_offset;

  float wave1 = std::sin(x * 0.3f + t) * 0.02f;
  float wave2 = std::sin(y * 0.21f + t * 1.3f) * 0.015f;
  float wave3 = std::sin((x + y) * 0.15f + t * 0.8f) * 0.01f;

  return base_z + wave1 + wave2 + wave3;
}

static void draw_water_quad(std::vector<uint32_t> &pixels, int width,
                            int height, float x0, float y0, float x1, float y1,
                            float x2, float y2, float x3, float y3,
                            uint32_t color) {
  float min_x = std::min({x0, x1, x2, x3});
  float max_x = std::max({x0, x1, x2, x3});
  float min_y = std::min({y0, y1, y2, y3});
  float max_y = std::max({y0, y1, y2, y3});

  for (int py = (int)min_y; py <= (int)max_y; ++py) {
    if (py < 0 || py >= height)
      continue;

    for (int px = (int)min_x; px <= (int)max_x; ++px) {
      if (px < 0 || px >= width)
        continue;

      pixels[py * width + px] = color;
    }
  }
}

void render_water(std::vector<uint32_t> &pixels, int view_width,
                  int view_height, const std::vector<WaterBody> &water_bodies,
                  float offset_x, float offset_y, const IsometricParams &params,
                  float time) {

  uint32_t water_base = 0xFF4488CC; // Light blue-ish

  for (const auto &water : water_bodies) {
    int gw = water.mesh.grid_width;
    int gh = water.mesh.grid_height;

    for (int gy = 0; gy < gh - 1; ++gy) {
      for (int gx = 0; gx < gw - 1; ++gx) {
        int i0 = gy * gw + gx;
        int i1 = gy * gw + (gx + 1);
        int i2 = (gy + 1) * gw + (gx + 1);
        int i3 = (gy + 1) * gw + gx;

        const auto &v0 = water.mesh.vertices[i0];
        const auto &v1 = water.mesh.vertices[i1];
        const auto &v2 = water.mesh.vertices[i2];
        const auto &v3 = water.mesh.vertices[i3];

        float z0 =
            get_water_height(v0.x, v0.y, v0.base_z, time, water.time_offset);
        float z1 =
            get_water_height(v1.x, v1.y, v1.base_z, time, water.time_offset);
        float z2 =
            get_water_height(v2.x, v2.y, v2.base_z, time, water.time_offset);
        float z3 =
            get_water_height(v3.x, v3.y, v3.base_z, time, water.time_offset);

        float iso_x0, iso_y0, iso_x1, iso_y1, iso_x2, iso_y2, iso_x3, iso_y3;
        world_to_iso(v0.x, v0.y, z0, iso_x0, iso_y0, params);
        world_to_iso(v1.x, v1.y, z1, iso_x1, iso_y1, params);
        world_to_iso(v2.x, v2.y, z2, iso_x2, iso_y2, params);
        world_to_iso(v3.x, v3.y, z3, iso_x3, iso_y3, params);

        iso_x0 += offset_x;
        iso_y0 += offset_y;
        iso_x1 += offset_x;
        iso_y1 += offset_y;
        iso_x2 += offset_x;
        iso_y2 += offset_y;
        iso_x3 += offset_x;
        iso_y3 += offset_y;

        float avg_z = (z0 + z1 + z2 + z3) * 0.25f;
        float depth_factor = 0.9f + (avg_z - v0.base_z) * 2.0f;
        depth_factor = std::clamp(depth_factor, 0.8f, 1.1f);

        uint8_t r = ((water_base >> 16) & 0xFF) * depth_factor;
        uint8_t g = ((water_base >> 8) & 0xFF) * depth_factor;
        uint8_t b = (water_base & 0xFF) * depth_factor;
        uint32_t color = 0xFF000000 | (r << 16) | (g << 8) | b;

        draw_water_quad(pixels, view_width, view_height, iso_x0, iso_y0, iso_x1,
                        iso_y1, iso_x2, iso_y2, iso_x3, iso_y3, color);
      }
    }
  }

  SDL_Log("Rendered %zu water bodies", water_bodies.size());
}
