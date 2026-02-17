#pragma once
#include <cstdint>
#include <vector>

struct Vec2 {
  float x, y;
};

struct IsoVec2 {
  float x, y;
};

struct PixelBuffer {
  std::vector<uint32_t> pixels;
  int width;
  int height;
};
