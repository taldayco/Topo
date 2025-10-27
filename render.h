// render.h
#pragma once
#include "contour.h"
#include "detail.h"
#include "isometric.h"
#include "palettes.h"
#include <SDL3/SDL.h>
#include <span>

struct TextureHandle {
  SDL_GPUTexture *texture;
  SDL_GPUSampler *sampler;
  int width;
  int height;
};

TextureHandle create_texture_from_heightmap(
    SDL_GPUDevice *device, std::span<const float> heightmap,
    std::span<const Line> contour_lines, int width, int height,
    bool use_isometric, const Palette &palette,
    const DetailParams &detail_params, float contour_opacity);

void release_texture(SDL_GPUDevice *device, const TextureHandle &handle);
