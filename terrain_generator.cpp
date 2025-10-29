#include "terrain_generator.h"
#include "basalt.h"
#include "config.h"
#include "plateau.h"
#include "water.h"
#include <SDL3/SDL.h>

TerrainGenerator::TerrainData
TerrainGenerator::generate(std::span<const float> heightmap, int width,
                           int height) {

  TerrainData data;

  data.plateaus = detect_plateaus(heightmap, width, height);
  SDL_Log("TerrainGenerator: Found %zu plateaus", data.plateaus.size());

  data.columns = generate_basalt_columns(
      heightmap, width, height, Config::HEX_SIZE, data.plateaus_with_columns);
  SDL_Log("TerrainGenerator: Generated %zu columns on %zu plateaus",
          data.columns.size(), data.plateaus_with_columns.size());

  data.water_bodies = identify_water_bodies(
      heightmap, width, height, data.plateaus, data.plateaus_with_columns);
  SDL_Log("TerrainGenerator: Identified %zu water bodies",
          data.water_bodies.size());

  return data;
}
