#include "background.h"
#include <SDL3/SDL.h>
#include <vector>
#include <string>

static std::vector<uint8_t> load_shader_file(const char *path) {
    SDL_IOStream *io = SDL_IOFromFile(path, "rb");
    if (!io) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BackgroundRenderer: Failed to open shader: %s", path);
        return {};
    }
    Sint64 size = SDL_GetIOSize(io);
    if (size <= 0) { SDL_CloseIO(io); return {}; }
    std::vector<uint8_t> data(size);
    SDL_ReadIO(io, data.data(), size);
    SDL_CloseIO(io);
    return data;
}

static SDL_GPUShader *create_shader(SDL_GPUDevice *device, const char *path,
                                    SDL_GPUShaderStage stage,
                                    Uint32 num_uniform_buffers) {
    auto code = load_shader_file(path);
    if (code.empty()) return nullptr;

    SDL_GPUShaderCreateInfo info = {};
    info.code        = code.data();
    info.code_size   = code.size();
    info.entrypoint  = "main";
    info.format      = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage       = stage;
    info.num_uniform_buffers = num_uniform_buffers;

    SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
    if (!shader)
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BackgroundRenderer: Failed to create shader %s: %s",
                     path, SDL_GetError());
    return shader;
}

bool BackgroundRenderer::init(SDL_GPUDevice *device,
                              SDL_GPUTextureFormat swapchain_format,
                              SDL_GPUTextureFormat depth_format) {
    this->device = device;

#ifdef SHADER_DIR
    std::string shader_dir = SHADER_DIR;
#else
    std::string shader_dir = "shaders";
#endif


    SDL_GPUShader *vert = create_shader(
        device, (shader_dir + "/background.vert.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_VERTEX, 0);
    SDL_GPUShader *frag = create_shader(
        device, (shader_dir + "/background.frag.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_FRAGMENT, 1);

    if (!vert || !frag) {
        if (vert) SDL_ReleaseGPUShader(device, vert);
        if (frag) SDL_ReleaseGPUShader(device, frag);
        return false;
    }

    SDL_GPUColorTargetDescription color_desc = {};
    color_desc.format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo pi = {};
    pi.vertex_shader   = vert;
    pi.fragment_shader = frag;
    pi.primitive_type  = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;




    pi.target_info.color_target_descriptions = &color_desc;
    pi.target_info.num_color_targets         = 1;
    pi.target_info.has_depth_stencil_target  = true;
    pi.target_info.depth_stencil_format      = depth_format;

    pi.depth_stencil_state.enable_depth_test  = false;
    pi.depth_stencil_state.enable_depth_write = false;

    pipeline = SDL_CreateGPUGraphicsPipeline(device, &pi);

    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!pipeline) {
        SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                     "BackgroundRenderer: Failed to create pipeline: %s",
                     SDL_GetError());
        return false;
    }

    SDL_Log("BackgroundRenderer: Pipeline created successfully");
    return true;
}

void BackgroundRenderer::draw(SDL_GPUCommandBuffer *cmd,
                              SDL_GPURenderPass   *render_pass,
                              float                time,
                              float                cam_x,
                              float                cam_y) {
    if (!pipeline || !render_pass) return;

    SDL_BindGPUGraphicsPipeline(render_pass, pipeline);

    struct { float time, cam_x, cam_y, pad; } u = { time, cam_x, cam_y, 0.0f };
    SDL_PushGPUFragmentUniformData(cmd, 0, &u, sizeof(u));

    SDL_DrawGPUPrimitives(render_pass, 3, 1, 0, 0);
}

void BackgroundRenderer::cleanup() {
    if (pipeline && device) {
        SDL_ReleaseGPUGraphicsPipeline(device, pipeline);
        pipeline = nullptr;
    }
}
