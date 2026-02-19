#pragma once
#include "terrain/contour.h"
#include "terrain/map_data.h"
#include <cstdint>
#include <span>
#include <vector>
struct HexColumn;
struct LavaBody;

// Positive terrain_map values (1..N) = plateau index + 1

class TerrainGenerator {
public:
  struct TerrainData {
    std::vector<Plateau> plateaus;
    std::vector<HexColumn> columns;
    std::vector<LavaBody> lava_bodies;
    std::vector<int> plateaus_with_columns;
    std::vector<int16_t> terrain_map;
  };

  static TerrainData generate(std::span<const float> heightmap,
                              std::span<const int> band_map, int width,
                              int height);
};
