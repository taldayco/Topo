#pragma once
#include <SDL3/SDL_gpu.h>

class BackgroundRenderer {
public:

    bool init(SDL_GPUDevice       *device,
              SDL_GPUTextureFormat swapchain_format,
              SDL_GPUTextureFormat depth_format);

    void draw(SDL_GPUCommandBuffer *cmd,
              SDL_GPURenderPass    *render_pass,
              float                 time,
              float                 cam_x,
              float                 cam_y);

    void cleanup();

private:
    SDL_GPUDevice            *device   = nullptr;
    SDL_GPUGraphicsPipeline  *pipeline = nullptr;
};
