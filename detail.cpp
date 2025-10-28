// detail.cpp
#include "detail.h"
#include <algorithm>
#include <cmath>

static uint32_t hash2d(int x, int y) {
  uint32_t h = (x * 374761393) ^ (y * 668265263);
  return (h ^ (h >> 13)) * 1274126177;
}

static int get_column_id(float x, float y, float hex_size) {
  const float sqrt3 = 1.732f;
  float col = x / (hex_size * sqrt3 * 0.5f);
  float row = y / hex_size - (int(col) & 1) * 0.5f;

  int icol = (int)std::round(col);
  int irow = (int)std::round(row);
  return icol * 10000 + irow;
}

static float dist_to_column_center(float x, float y, float hex_size) {
  const float sqrt3 = 1.732f;
  float col = x / (hex_size * sqrt3 * 0.5f);
  float row = y / hex_size - (int(col) & 1) * 0.5f;

  float cx = std::round(col) * hex_size * sqrt3 * 0.5f;
  float cy =
      std::round(row) * hex_size + (int(std::round(col)) & 1) * hex_size * 0.5f;

  float dx = x - cx;
  float dy = y - cy;
  return std::sqrt(dx * dx + dy * dy);
}

// Bayer 8x8 matrix for ordered dithering
static const float BAYER_MATRIX[8][8] = {
    {0 / 64.0f, 32 / 64.0f, 8 / 64.0f, 40 / 64.0f, 2 / 64.0f, 34 / 64.0f,
     10 / 64.0f, 42 / 64.0f},
    {48 / 64.0f, 16 / 64.0f, 56 / 64.0f, 24 / 64.0f, 50 / 64.0f, 18 / 64.0f,
     58 / 64.0f, 26 / 64.0f},
    {12 / 64.0f, 44 / 64.0f, 4 / 64.0f, 36 / 64.0f, 14 / 64.0f, 46 / 64.0f,
     6 / 64.0f, 38 / 64.0f},
    {60 / 64.0f, 28 / 64.0f, 52 / 64.0f, 20 / 64.0f, 62 / 64.0f, 30 / 64.0f,
     54 / 64.0f, 22 / 64.0f},
    {3 / 64.0f, 35 / 64.0f, 11 / 64.0f, 43 / 64.0f, 1 / 64.0f, 33 / 64.0f,
     9 / 64.0f, 41 / 64.0f},
    {51 / 64.0f, 19 / 64.0f, 59 / 64.0f, 27 / 64.0f, 49 / 64.0f, 17 / 64.0f,
     57 / 64.0f, 25 / 64.0f},
    {15 / 64.0f, 47 / 64.0f, 7 / 64.0f, 39 / 64.0f, 13 / 64.0f, 45 / 64.0f,
     5 / 64.0f, 37 / 64.0f},
    {63 / 64.0f, 31 / 64.0f, 55 / 64.0f, 23 / 64.0f, 61 / 64.0f, 29 / 64.0f,
     53 / 64.0f, 21 / 64.0f}};

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

      uint8_t r_out = std::clamp(
          (int)(((pixel >> 16) & 0xFF) * (1.0f + threshold)), 0, 255);
      uint8_t g_out =
          std::clamp((int)(((pixel >> 8) & 0xFF) * (1.0f + threshold)), 0, 255);
      uint8_t b_out =
          std::clamp((int)((pixel & 0xFF) * (1.0f + threshold)), 0, 255);

      pixels[idx] = 0xFF000000 | (r_out << 16) | (g_out << 8) | b_out;
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
        uint8_t r = ((base >> 16) & 0xFF) * (1.0f - cliff_darkness);
        uint8_t g = ((base >> 8) & 0xFF) * (1.0f - cliff_darkness);
        uint8_t b = (base & 0xFF) * (1.0f - cliff_darkness);
        pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
        continue;
      }

      float dist = dist_to_column_center(x, y, hex_size);

      if (dist > hex_size * 0.4f) {
        uint32_t base = pixels[idx];
        float edge_factor = (dist - hex_size * 0.4f) / (hex_size * 0.1f);
        edge_factor = std::clamp(edge_factor, 0.0f, 1.0f);

        uint8_t r = ((base >> 16) & 0xFF) * (1.0f - edge_factor * 0.4f);
        uint8_t g = ((base >> 8) & 0xFF) * (1.0f - edge_factor * 0.4f);
        uint8_t b = (base & 0xFF) * (1.0f - edge_factor * 0.4f);
        pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
      } else {
        float shade = 1.0f - (dist / hex_size) * 0.15f;

        uint32_t base = pixels[idx];
        uint8_t r = std::min(255, (int)(((base >> 16) & 0xFF) * shade));
        uint8_t g = std::min(255, (int)(((base >> 8) & 0xFF) * shade));
        uint8_t b = std::min(255, (int)((base & 0xFF) * shade));
        pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
      }

      int col_id = get_column_id(x, y, hex_size);
      uint32_t h_val = hash2d(col_id, 0);
      float variation = ((h_val & 0xFF) / 255.0f - 0.5f) * 0.08f;

      uint32_t base = pixels[idx];
      int r =
          std::clamp((int)(((base >> 16) & 0xFF) * (1.0f + variation)), 0, 255);
      int g =
          std::clamp((int)(((base >> 8) & 0xFF) * (1.0f + variation)), 0, 255);
      int b = std::clamp((int)((base & 0xFF) * (1.0f + variation)), 0, 255);
      pixels[idx] = 0xFF000000 | (r << 16) | (g << 8) | b;
    }
  }

  apply_dither(pixels, width, height, 0.15f);
}
