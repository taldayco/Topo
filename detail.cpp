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

// Hexagonal grid coordinates
static void hex_coords(int x, int y, float &hx, float &hy) {
  const float HEX_SIZE = 8.0f;
  hx = x * HEX_SIZE * 0.866f; // sqrt(3)/2
  hy = y * HEX_SIZE - (x % 2) * HEX_SIZE * 0.5f;
}

static float hex_distance(float x, float y, float hx, float hy) {
  float dx = x - hx;
  float dy = y - hy;

  // Hexagonal distance approximation
  float dist = std::abs(dx) * 0.866f + std::abs(dy) * 0.5f;
  dist = std::max(dist, std::abs(dy));

  return dist;
}

static void draw_basalt_columns(std::vector<uint32_t> &pixels,
                                std::span<const float> heightmap, int width,
                                int height, const Palette &palette) {
  const float HEX_SIZE = 8.0f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      float h = heightmap[idx];

      // Find nearest hex center
      int hx_grid = (int)(x / (HEX_SIZE * 0.866f));
      int hy_grid = (int)(y / HEX_SIZE);

      float min_dist = 1e9f;

      // Check surrounding hex centers
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          float hx, hy;
          hex_coords(hx_grid + dx, hy_grid + dy, hx, hy);
          float dist = hex_distance(x, y, hx, hy);
          min_dist = std::min(min_dist, dist);
        }
      }

      // Darken edges of hexagons
      uint32_t base = pixels[idx];
      if (min_dist > HEX_SIZE - 1.5f) {
        uint8_t r = (base >> 16) & 0xFF;
        uint8_t g = (base >> 8) & 0xFF;
        uint8_t b = base & 0xFF;

        r *= 0.7f;
        g *= 0.7f;
        b *= 0.7f;

        pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
      }
    }
  }
}

static void add_elevation_steps(std::vector<uint32_t> &pixels,
                                std::span<const float> heightmap, int width,
                                int height) {
  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      int idx = y * width + x;
      float h = heightmap[idx];

      // Check for elevation drop (step edge)
      float h_right = heightmap[y * width + (x + 1)];
      float h_down = heightmap[(y + 1) * width + x];

      bool is_edge = (h - h_right > 0.05f) || (h - h_down > 0.05f);

      if (is_edge) {
        uint32_t base = pixels[idx];
        uint8_t r = (base >> 16) & 0xFF;
        uint8_t g = (base >> 8) & 0xFF;
        uint8_t b = base & 0xFF;

        // Darken step edges significantly
        r *= 0.4f;
        g *= 0.4f;
        b *= 0.4f;

        pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
      }
    }
  }
}

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params) {

  // Add basalt column texture
  draw_basalt_columns(pixels, heightmap, width, height, palette);

  // Darken elevation step edges
  add_elevation_steps(pixels, heightmap, width, height);
}
