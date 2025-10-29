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

  auto channel_regions =
      extract_channel_spaces(data.columns, width, height, heightmap);
  SDL_Log("TerrainGenerator: Found %zu channel regions",
          channel_regions.size());

  auto water_channels =
      filter_water_channels(channel_regions, heightmap, width, height);
  SDL_Log("TerrainGenerator: Selected %zu water channels",
          water_channels.size());
  data.water_bodies =
      channels_to_water_bodies(water_channels, heightmap, width, height);
  SDL_Log("TerrainGenerator: Created %zu water bodies",
          data.water_bodies.size());

  return data;
}
