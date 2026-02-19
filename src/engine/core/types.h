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

struct ViewState {
  float zoom = 1.0f;
  float pan_x = 0.5f, pan_y = 0.5f;
};
