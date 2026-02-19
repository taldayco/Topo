#include "terrain/detail.h"
#include "terrain/color.h"
#include <algorithm>
#include <cmath>

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
        float cliff_darkness = std::min(max_drop * 8.0f, 0.8f);
        pixels[idx] = darken_color(pixels[idx], cliff_darkness);
      }
    }
  }

  apply_hex_dither(pixels, width, height, params.dither_strength);
}
