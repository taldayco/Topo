#pragma once
#include "contour.h"
#include <span>
#include <vector>
struct HexColumn;
struct WaterBody;

class TerrainGenerator {
public:
  struct TerrainData {
    std::vector<Plateau> plateaus;
    std::vector<HexColumn> columns;
    std::vector<WaterBody> water_bodies;
    std::vector<int> plateaus_with_columns;
  };

  static TerrainData generate(std::span<const float> heightmap,
                              std::span<const int> band_map, int width,
                              int height);
};
