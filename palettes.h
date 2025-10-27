// palettes.h
#pragma once
#include <algorithm>
#include <cstdint>

struct Palette {
  const char *name;
  uint32_t colors[6]; // base, dark, mid, light, accent, line
};

static const Palette PALETTES[] = {
    {"Grayscale",
     {0xFF1A1A1A, 0xFF3A3A3A, 0xFF6A6A6A, 0xFF9A9A9A, 0xFFCACACA, 0xFF000000}}};

static constexpr int PALETTE_COUNT = sizeof(PALETTES) / sizeof(Palette);

inline uint32_t lerp_color(uint32_t c1, uint32_t c2, float t) {
  uint8_t r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
  uint8_t r2 = (c2 >> 16) & 0xFF, g2 = (c2 >> 8) & 0xFF, b2 = c2 & 0xFF;

  uint8_t r = r1 + (r2 - r1) * t;
  uint8_t g = g1 + (g2 - g1) * t;
  uint8_t b = b1 + (b2 - b1) * t;

  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

inline uint32_t get_elevation_color_smooth(float h, const Palette &p) {
  h = std::clamp(h, 0.0f, 1.0f);

  if (h < 0.25f) {
    float t = h / 0.25f;
    return lerp_color(p.colors[0], p.colors[1], t);
  } else if (h < 0.5f) {
    float t = (h - 0.25f) / 0.25f;
    return lerp_color(p.colors[1], p.colors[2], t);
  } else if (h < 0.75f) {
    float t = (h - 0.5f) / 0.25f;
    return lerp_color(p.colors[2], p.colors[3], t);
  } else {
    float t = (h - 0.75f) / 0.25f;
    return lerp_color(p.colors[3], p.colors[4], t);
  }
}

inline uint32_t add_noise_variation(uint32_t color, int x, int y,
                                    float strength = 0.08f) {
  uint32_t hash = x * 374761393 + y * 668265263;
  hash = (hash ^ (hash >> 13)) * 1274126177;
  float noise = ((hash & 0xFFFF) / 65535.0f - 0.5f) * strength;

  int r = std::clamp((int)((color >> 16 & 0xFF) * (1.0f + noise)), 0, 255);
  int g = std::clamp((int)((color >> 8 & 0xFF) * (1.0f + noise)), 0, 255);
  int b = std::clamp((int)((color & 0xFF) * (1.0f + noise)), 0, 255);

  return 0xFF000000 | (r << 16) | (g << 8) | b;
}

inline uint32_t organic_color(float h, int x, int y, const Palette &p) {
  uint32_t base = get_elevation_color_smooth(h, p);
  return add_noise_variation(base, x, y);
}

inline uint32_t get_elevation_color(float h, const Palette &p) {
  if (h < 0.2f)
    return p.colors[0];
  if (h < 0.4f)
    return p.colors[1];
  if (h < 0.6f)
    return p.colors[2];
  if (h < 0.8f)
    return p.colors[3];
  return p.colors[4];
}
