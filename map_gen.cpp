#include "map_gen.h"
#include "delve_render.h"
#include "palettes.h"

void regenerate_map(AppState &state, SDL_GPUDevice *device,
                    TextureHandle &map_texture) {
  SDL_Log("Starting regeneration...");
  auto start = SDL_GetTicks();

  generate_heightmap(state.heightmap, Config::MAP_WIDTH, Config::MAP_HEIGHT,
                     state.noise_params, state.map_scale);

  auto after_heightmap = SDL_GetTicks();
  SDL_Log("Heightmap: %llu ms", after_heightmap - start);

  state.contour_lines.clear();
  extract_contours(state.heightmap, Config::MAP_WIDTH, Config::MAP_HEIGHT,
                   state.contour_interval, state.contour_lines, state.band_map);

  auto after_contours = SDL_GetTicks();
  SDL_Log("Contours (%zu lines): %llu ms", state.contour_lines.size(),
          after_contours - after_heightmap);

  if (map_texture.texture) {
    release_texture(device, map_texture);
  }

  SDL_Log("Creating texture with lines...");
  PixelBuffer buf = generate_map_pixels(
      state.heightmap, state.band_map, state.contour_lines, Config::MAP_WIDTH,
      Config::MAP_HEIGHT, state.use_isometric,
      PALETTES[state.current_palette], state.detail_params,
      state.contour_opacity, state.iso_padding, state.iso_offset_x_adjust,
      state.iso_offset_y_adjust);
  map_texture =
      upload_pixels_to_texture(device, buf.pixels.data(), buf.width, buf.height);
  SDL_Log("Texture created");

  SDL_WaitForGPUIdle(device);

  auto end = SDL_GetTicks();
  SDL_Log("Total regeneration: %llu ms\n", end - start);

  state.need_regenerate = false;
}
