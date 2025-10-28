#include "detail.h"
#include <algorithm>
#include <cmath>

static uint32_t hash2d(int x, int y) {
  uint32_t h = (x * 374761393) ^ (y * 668265263);
  return (h ^ (h >> 13)) * 1274126177;
}

static void apply_dither(std::vector<uint32_t> &pixels, int width, int height,
                         float strength) {
  const float HEX_SIZE = 8.0f;
  const float sqrt3 = 1.732f;

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int idx = y * width + x;
      uint32_t pixel = pixels[idx];

      float q = x / (HEX_SIZE * sqrt3);
      float r = y / HEX_SIZE - q * 0.5f;

      int iq = (int)std::round(q);
      int ir = (int)std::round(r);
      int is = (int)std::round(-q - r);

      float dq = std::abs(iq - q);
      float dr = std::abs(ir - r);
      float ds = std::abs(is - (-q - r));

      if (dq > dr && dq > ds) {
        iq = -ir - is;
      } else if (dr > ds) {
        ir = -iq - is;
      }

      uint32_t hash = ((uint32_t)iq * 374761393) ^ ((uint32_t)ir * 668265263);
      float threshold = ((hash & 0xFF) / 255.0f - 0.5f) * strength;

      uint8_t rc = std::clamp(
          (int)(((pixel >> 16) & 0xFF) * (1.0f + threshold)), 0, 255);
      uint8_t gc =
          std::clamp((int)(((pixel >> 8) & 0xFF) * (1.0f + threshold)), 0, 255);
      uint8_t bc =
          std::clamp((int)((pixel & 0xFF) * (1.0f + threshold)), 0, 255);

      pixels[idx] = 0xFF000000 | (rc << 16) | (gc << 8) | bc;
    }
  }
}

void add_procedural_details(std::vector<uint32_t> &pixels,
                            std::span<const float> heightmap, int width,
                            int height, const Palette &palette,
                            const DetailParams &params) {
  const float hex_size = params.column_size;

  for (int y = 1; y < height - 1; ++y) {
    for (int x = 1; x < width - 1; ++x) {
      int idx = y * width + x;
      float h = heightmap[idx];

      float h_right = heightmap[y * width + (x + 1)];
      float h_down = heightmap[(y + 1) * width + x];
      float max_drop = std::max(h - h_right, h - h_down);

      if (max_drop > 0.05f) {
        uint32_t base = pixels[idx];
        float cliff_darkness = std::min(max_drop * 8.0f, 0.8f);
        uint8_t rc = ((base >> 16) & 0xFF) * (1.0f - cliff_darkness);
        uint8_t gc = ((base >> 8) & 0xFF) * (1.0f - cliff_darkness);
        uint8_t bc = (base & 0xFF) * (1.0f - cliff_darkness);
        pixels[idx] = 0xFF000000 | (rc << 16) | (gc << 8) | bc;
      }
    }
  }

  apply_dither(pixels, width, height, 0.15f);
}
