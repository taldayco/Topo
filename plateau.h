#pragma once
#include <span>
#include <vector>

struct Plateau {
  float height;
  std::vector<int> pixels;
  float center_x, center_y;
};

std::vector<Plateau> detect_plateaus(std::span<const float> heightmap,
                                     int width, int height);
