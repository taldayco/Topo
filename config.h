#pragma once
#include <stdint.h>

struct Config {
  // Map dimensions
  static constexpr int MAP_WIDTH = 512;
  static constexpr int MAP_HEIGHT = 512;

  // Window dimensions
  static constexpr int WINDOW_WIDTH = 1400;
  static constexpr int WINDOW_HEIGHT = 1024;
  static constexpr int UI_PANEL_WIDTH = 376;

  // Map generation
  static constexpr float HEX_SIZE = 8.0f;
  static constexpr float WATER_GRID_SPACING = 10.0f;
  static constexpr float HEIGHT_THRESHOLD = 0.02f;
  static constexpr int MIN_PLATEAU_SIZE = 50;
  static constexpr float DEFAULT_CONTOUR_INTERVAL = 0.05f;
  static constexpr float DEFAULT_ISO_PADDING = 50.0f;

  // Noise parameters
  static constexpr float DEFAULT_NOISE_SCALE = 0.003f;
  static constexpr int DEFAULT_NOISE_OCTAVES = 6;
  static constexpr float DEFAULT_NOISE_LACUNARITY = 2.0f;
  static constexpr float DEFAULT_NOISE_GAIN = 0.5f;
  static constexpr int DEFAULT_NOISE_SEED = 1337;
  static constexpr int DEFAULT_NOISE_LEVELS = 8;

  // Isometric projection
  static constexpr float ISO_TILE_WIDTH = 2.0f;
  static constexpr float ISO_TILE_HEIGHT = 1.0f;
  static constexpr float ISO_HEIGHT_SCALE = 100.0f;

  // Rendering
  static constexpr float GRADIENT_SCALE = 2.0f;
  static constexpr uint32_t BACKGROUND_COLOR = 0xFF2D2D30;
  static constexpr uint32_t WATER_COLOR = 0xFF4488CC;
  static constexpr float DEFAULT_CONTOUR_OPACITY = 0.25f;
};
