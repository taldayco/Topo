#pragma once
#include <stdint.h>

struct Config {
  // Map generation
  static constexpr float HEX_SIZE = 8.0f;
  static constexpr float WATER_GRID_SPACING = 10.0f;
  static constexpr float HEIGHT_THRESHOLD = 0.02f;
  static constexpr int MIN_PLATEAU_SIZE = 50;

  // Rendering
  static constexpr float GRADIENT_SCALE = 2.0f;
  static constexpr uint32_t BACKGROUND_COLOR = 0xFF2D2D30;
  static constexpr uint32_t WATER_COLOR = 0xFF4488CC;
};
