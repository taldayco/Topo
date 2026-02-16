#pragma once
#include "app_state.h"
#include "gpu.h"
#include <SDL3/SDL.h>

void ui_init(SDL_Window *window, SDL_GPUDevice *device);
void ui_shutdown();
void ui_process_event(SDL_Event &event);
void ui_render(AppState &state, const TextureHandle &map_texture);
void ui_prepare_draw(SDL_GPUCommandBuffer *cmd);
void ui_draw(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render_pass);
