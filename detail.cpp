// detail.cpp
#include "detail.h"
#include <algorithm>
#include <cmath>

static float get_slope(std::span<const float> heightmap, int x, int y,
                       int width, int height) {
  if (x == 0 || x == width - 1 || y == 0 || y == height - 1)
    return 0;

  float dx = heightmap[y * width + (x + 1)] - heightmap[y * width + (x - 1)];
  float dy = heightmap[(y + 1) * width + x] - heightmap[(y - 1) * width + x];

  return std::sqrt(dx * dx + dy * dy);
}

static bool should_place_rock(float height, float slope, int x, int y,
                              float density) {
  if (slope < 0.15f)
    return false;

  uint32_t hash = (x * 73856093) ^ (y * 19349663);
  float noise = (hash & 0xFF) / 255.0f;

  return noise > (1.0f - slope * 2.0f * density);
}

static void draw_rock(std::vector<uint32_t> &pixels, int width, int height,
                      int x, int y) {
  uint32_t rock_color = 0xFF0A0A0A; // Much darker - almost black

  // 3x3 for visibility
  for (int dy = -1; dy <= 1; ++dy) {
    for (int dx = -1; dx <= 1; ++dx) {
      int px = x + dx, py = y + dy;
      if (px >= 0 && px < width && py >= 0 && py < height) {
        pixels[py * width + px] = rock_color;
      }
    }
  }
}

static void draw_moss(std::vector<uint32_t> &pixels, int width, int height,
                      int x, int y, float h, const Palette &palette) {
  // Much brighter - 1.5x instead of 1.15x
  uint32_t base = get_elevation_color_smooth(h, palette);
  uint8_t r = std::min(255, (int)((base >> 16 & 0xFF) * 1.5f));
  uint8_t g = std::min(255, (int)((base >> 8 & 0xFF) * 1.5f));
  uint8_t b = std::min(255, (int)((base & 0xFF) * 1.5f));

  if (x >= 0 && x < width && y >= 0 && y < height) {
    pixels[y * width + x] = 0xFF000000 | (r << 16) | (g << 8) | b;
  }
}

static void draw_grass(std::vector<uint32_t> &pixels, int width, int height,
                       int x, int y) {
  uint32_t grass_color = 0xFFB0B0B0; // Much brighter

  for (int i = 0; i < 3; ++i) {
    int py = y - i;
    if (py >= 0 && py < height) {
      pixels[py * width + x] = grass_color;
    }
  }
}

// Also reduce density - currently placing on EVERY valid pixel
static bool should_place_moss(float height, float slope, int x, int y,
                              float density) {
  if (height < 0.3f || height > 0.7f)
    return false;
  if (slope > 0.1f)
    return false;

  uint32_t hash = (x * 19349663) ^ (y * 83492791);
  return (hash & 0xFFF) < 100 * density; // Much sparser - ~2.4% coverage
}

static bool should_place_grass(float height, float slope, int x, int y,
                               float density) {
  if (height > 0.4f)
    return false;
  if (slope > 0.08f)
    return false;

  uint32_t hash = (x * 83492791) ^ (y * 73856093);
  return (hash & 0xFFF) < 200 * density; // ~4.8% coverage
}

static void add_slope_hatching(std::vector<uint32_t> &pixels,
                               std::span<const float> heightmap, int width,
                               int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      float slope = get_slope(heightmap, x, y, width, height);

      if (slope > 0.12f && (x + y) % 3 == 0) {
        pixels[y * width + x] = 0xFF2A2A2A;
      }
    }
  }
}

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params) {
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      int idx = y * width + x;
      float h = heightmap[idx];
      float slope = get_slope(heightmap, x, y, width, height);

      if (params.enable_grass &&
          should_place_grass(h, slope, x, y, params.grass_density)) {
        draw_grass(pixels, width, height, x, y);
      }

      if (params.enable_moss &&
          should_place_moss(h, slope, x, y, params.moss_density)) {
        draw_moss(pixels, width, height, x, y, h, palette);
      }

      if (params.enable_rocks &&
          should_place_rock(h, slope, x, y, params.rock_density)) {
        draw_rock(pixels, width, height, x, y);
      }
    }
  }

  if (params.enable_hatching) {
    add_slope_hatching(pixels, heightmap, width, height);
  }
}
