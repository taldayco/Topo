#pragma once
#include "terrain/map_data.h"
#include "terrain/noise_cache.h"
#include "terrain/noise_layers.h"

struct CompositionParams {
  float river_elevation_max = 0.35f;
  float lava_threshold = 0.3f;
  int terrace_levels = 8;
  int min_region_size = 153;
};

void compose_layers(MapData &data, const ElevationParams &elev,
                    const RiverParams &river, const WorleyParams &worley,
                    const CompositionParams &comp,
                    NoiseCache *cache = nullptr);
