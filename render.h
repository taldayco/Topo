#pragma once
#include "contour.h"
#include <SDL3/SDL.h>
#include <span>

struct TextureHandle {
  SDL_GPUTexture *texture;
  SDL_GPUSampler *sampler;
};

TextureHandle create_texture_from_heightmap(SDL_GPUDevice *device,
                                            std::span<const float> heightmap,
                                            std::span<const Line> contour_lines,
                                            int width, int height);

void release_texture(SDL_GPUDevice *device, const TextureHandle &handle);
