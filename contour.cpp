#include "contour.h"

void extract_contours(std::span<const float> heightmap, int width, int height,
                      float interval, std::vector<Line> &out_lines) {
  out_lines.clear();

  // Quantize heightmap to discrete levels
  std::vector<int> levels(width * height);
  for (int i = 0; i < width * height; ++i) {
    levels[i] = (int)(heightmap[i] / interval);
  }

  // Edge detection: find boundaries between different levels
  for (int y = 0; y < height - 1; ++y) {
    for (int x = 0; x < width - 1; ++x) {
      int level00 = levels[y * width + x];
      int level10 = levels[y * width + x + 1];
      int level01 = levels[(y + 1) * width + x];

      // Horizontal edge
      if (level00 != level10) {
        out_lines.push_back(
            {(float)x + 0.5f, (float)y, (float)x + 0.5f, (float)y + 1});
      }

      // Vertical edge
      if (level00 != level01) {
        out_lines.push_back(
            {(float)x, (float)y + 0.5f, (float)x + 1, (float)y + 0.5f});
      }
    }
  }
}
