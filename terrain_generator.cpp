#include "terrain_generator.h"
#include "basalt.h"
#include "config.h"
#include "water.h"
#include <SDL3/SDL.h>
#include <algorithm>
#include <queue>

static std::vector<UnusedRegion>
detect_unused_regions(std::span<const int16_t> terrain_map,
                      std::span<const float> heightmap, int width, int height) {
  std::vector<uint8_t> visited(width * height, 0);
  std::vector<UnusedRegion> regions;
  const int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

  for (int sy = 0; sy < height; ++sy) {
    for (int sx = 0; sx < width; ++sx) {
      int start = sy * width + sx;
      if (terrain_map[start] < TERRAIN_EMPTY || visited[start])
        continue;

      UnusedRegion region;
      region.min_x = (float)sx;
      region.max_x = (float)sx;
      region.min_y = (float)sy;
      region.max_y = (float)sy;
      float sum_h = 0;

      std::queue<int> q;
      q.push(start);
      visited[start] = 1;

      while (!q.empty()) {
        int idx = q.front();
        q.pop();
        int cx = idx % width, cy = idx / width;
        region.pixels.push_back(idx);
        sum_h += heightmap[idx];

        region.min_x = std::min(region.min_x, (float)cx);
        region.max_x = std::max(region.max_x, (float)cx);
        region.min_y = std::min(region.min_y, (float)cy);
        region.max_y = std::max(region.max_y, (float)cy);

        for (auto [dx, dy] : dirs) {
          int nx = cx + dx, ny = cy + dy;
          if (nx < 0 || ny < 0 || nx >= width || ny >= height)
            continue;
          int nidx = ny * width + nx;
          if (!visited[nidx] && terrain_map[nidx] >= TERRAIN_EMPTY) {
            visited[nidx] = 1;
            q.push(nidx);
          }
        }
      }

      region.avg_elevation = sum_h / region.pixels.size();

      if ((int)region.pixels.size() >= 50)
        regions.push_back(std::move(region));
    }
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

  // Water detection reads terrain_map for gaps (TERRAIN_EMPTY)
  auto channel_regions =
      extract_channel_spaces(data.terrain_map, width, height, heightmap);
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

  // Mark water pixels in terrain_map
  for (const auto& wb : data.water_bodies)
    for (int idx : wb.pixels)
      if (idx >= 0 && idx < width * height)
        data.terrain_map[idx] = TERRAIN_WATER;

  // Detect unused regions (flood-fill on TERRAIN_EMPTY)
  data.unused_regions = detect_unused_regions(data.terrain_map, heightmap,
                                               width, height);
  SDL_Log("TerrainGenerator: Found %zu unused regions (>= 50 pixels)",
          data.unused_regions.size());

  // Classify: largest 3 regions are Marble, rest are Void
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
      data.unused_regions[indices[i]].type = RegionType::Marble;
  }

  return data;
}
