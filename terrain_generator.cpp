#include "terrain_generator.h"
#include "basalt.h"
#include "config.h"
#include "flood_fill.h"
#include "lava.h"
#include <SDL3/SDL.h>
#include <algorithm>

static std::vector<UnusedRegion>
detect_unused_regions(std::span<const int16_t> terrain_map,
                      std::span<const float> heightmap, int width, int height) {
  auto pixel_groups = flood_fill_regions(
      width, height,
      [&](int idx) { return terrain_map[idx] >= TERRAIN_EMPTY; }, 50);

  std::vector<UnusedRegion> regions;
  regions.reserve(pixel_groups.size());

  for (auto &pixels : pixel_groups) {
    UnusedRegion region;
    region.pixels = std::move(pixels);

    float sum_h = 0;
    float min_x = 1e9f, max_x = -1e9f, min_y = 1e9f, max_y = -1e9f;
    for (int idx : region.pixels) {
      int cx = idx % width, cy = idx / width;
      sum_h += heightmap[idx];
      min_x = std::min(min_x, (float)cx);
      max_x = std::max(max_x, (float)cx);
      min_y = std::min(min_y, (float)cy);
      max_y = std::max(max_y, (float)cy);
    }
    region.avg_elevation = sum_h / region.pixels.size();
    region.min_x = min_x;
    region.max_x = max_x;
    region.min_y = min_y;
    region.max_y = max_y;

    regions.push_back(std::move(region));
  }

  return regions;
}

TerrainGenerator::TerrainData
TerrainGenerator::generate(std::span<const float> heightmap,
                           std::span<const int> band_map, int width,
                           int height) {

  TerrainData data;

  // Initialize terrain map
  data.terrain_map.assign(width * height, TERRAIN_EMPTY);

  // Plateau detection writes plateau IDs into terrain_map
  data.plateaus = detect_plateaus(band_map, heightmap, width, height,
                                  data.terrain_map);
  SDL_Log("TerrainGenerator: Found %zu plateaus", data.plateaus.size());

  // Column generation reads plateau IDs, writes TERRAIN_BASALT
  data.columns = generate_basalt_columns(heightmap, width, height,
                                          Config::HEX_SIZE, data.plateaus,
                                          data.plateaus_with_columns,
                                          data.terrain_map);
  SDL_Log("TerrainGenerator: Generated %zu columns on %zu plateaus",
          data.columns.size(), data.plateaus_with_columns.size());

  // Lava detection reads terrain_map for gaps (TERRAIN_EMPTY)
  auto channel_regions =
      extract_channel_spaces(data.terrain_map, width, height, heightmap);
  SDL_Log("TerrainGenerator: Found %zu channel regions",
          channel_regions.size());

  auto lava_channels =
      filter_lava_channels(channel_regions, heightmap, width, height);
  SDL_Log("TerrainGenerator: Selected %zu lava channels",
          lava_channels.size());
  data.lava_bodies =
      channels_to_lava_bodies(lava_channels, heightmap, width, height);
  SDL_Log("TerrainGenerator: Created %zu lava bodies",
          data.lava_bodies.size());

  // Mark lava pixels in terrain_map
  for (const auto& wb : data.lava_bodies)
    for (int idx : wb.pixels)
      if (idx >= 0 && idx < width * height)
        data.terrain_map[idx] = TERRAIN_LAVA;

  // Detect unused regions (flood-fill on TERRAIN_EMPTY)
  data.unused_regions = detect_unused_regions(data.terrain_map, heightmap,
                                               width, height);
  SDL_Log("TerrainGenerator: Found %zu unused regions (>= 50 pixels)",
          data.unused_regions.size());

  // Classify: largest 3 regions are UnboundSpace, rest are Crystal
  if (!data.unused_regions.empty()) {
    std::vector<size_t> indices(data.unused_regions.size());
    for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
    std::partial_sort(indices.begin(),
                      indices.begin() + std::min<size_t>(3, indices.size()),
                      indices.end(),
                      [&](size_t a, size_t b) {
                        return data.unused_regions[a].pixels.size() >
                               data.unused_regions[b].pixels.size();
                      });
    for (size_t i = 0; i < std::min<size_t>(3, indices.size()); ++i)
      data.unused_regions[indices[i]].type = RegionType::UnboundSpace;
  }

  return data;
}
