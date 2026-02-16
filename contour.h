#pragma once
#include <span>
#include <vector>

struct Line {
  float x1, y1, x2, y2;
  float elevation;
};

void extract_contours(std::span<const float> heightmap, int width, int height,
                      float interval, std::vector<Line> &out_lines,
                      std::vector<int> &out_band_map);

struct Plateau {
  float height;
  std::vector<int> pixels;
  float center_x, center_y;
};

std::vector<Plateau> detect_plateaus(std::span<const int> band_map,
                                     std::span<const float> heightmap,
                                     int width, int height);
