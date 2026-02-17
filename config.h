#pragma once
#include <stdint.h>

struct Config {
  // Toggle ImGui UI (false = plain SDL window with just the map)
  static constexpr bool use_IMGUI = true;

  // Toggle debug overlay for unused regions
  static inline bool enable_debug_overlay = true;

  // Map dimensions (base heightmap resolution for quality)
  static inline int MAP_WIDTH = 1024;
  static inline int MAP_HEIGHT = 1024;

  // Window dimensions (runtime-calculated)
  static inline int WINDOW_WIDTH = 1450;
  static inline int WINDOW_HEIGHT = 1024;
  static inline int UI_PANEL_WIDTH = 376;

  // Display size percentages
  static constexpr float WINDOW_WIDTH_PERCENT = 0.85f;  // 85% of display width
  static constexpr float WINDOW_HEIGHT_PERCENT = 0.85f; // 85% of display height
  static constexpr float UI_PANEL_WIDTH_PERCENT = 0.25f; // 25% of window width
  static constexpr int UI_PANEL_MIN_WIDTH = 400; // Minimum width for controls

  // Map scale (controls zoom level independent of resolution)
  static constexpr float DEFAULT_MAP_SCALE = 1.0f;

  // Map generation
  static constexpr float HEX_SIZE = 8.0f;
  static constexpr float LAVA_GRID_SPACING = 10.0f;
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
  static constexpr uint32_t LAVA_COLOR = 0xFF4488CC;
  static constexpr float DEFAULT_CONTOUR_OPACITY = 0.25f;
};
