#pragma once
#include "app_state.h"
#include "gpu.h"
#include <SDL3/SDL.h>

void regenerate_map(AppState &state, SDL_GPUDevice *device,
                    TextureHandle &map_texture);
