#pragma once
#include "contour.h"
#include <cstdint>
#include <span>
#include <vector>
struct HexColumn;
struct WaterBody;

// Terrain map pixel values
constexpr int16_t TERRAIN_EMPTY  =  0;
constexpr int16_t TERRAIN_BASALT = -1;
constexpr int16_t TERRAIN_WATER  = -2;
// Positive values (1..N) = plateau index + 1

enum class RegionType { Marble, Void };

struct UnusedRegion {
    std::vector<int> pixels;
    float avg_elevation;
    float min_x, max_x, min_y, max_y;
    RegionType type = RegionType::Void;
};

class TerrainGenerator {
public:
  struct TerrainData {
    std::vector<Plateau> plateaus;
    std::vector<HexColumn> columns;
    std::vector<WaterBody> water_bodies;
    std::vector<int> plateaus_with_columns;
    std::vector<int16_t> terrain_map;
    std::vector<UnusedRegion> unused_regions;
  };

  static TerrainData generate(std::span<const float> heightmap,
                              std::span<const int> band_map, int width,
                              int height);
};
