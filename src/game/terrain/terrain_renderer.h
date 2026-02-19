#pragma once
#include "terrain/terrain_mesh.h"
#include <SDL3/SDL.h>

class TerrainRenderer {
public:
  void init(SDL_GPUDevice *device, SDL_Window *window);
  void upload_mesh(SDL_GPUDevice *device, const TerrainMesh &mesh);
  void draw(SDL_GPURenderPass *render_pass, SDL_GPUCommandBuffer *cmd,
            const SceneUniforms &uniforms);
  void cleanup(SDL_GPUDevice *device);

  bool is_initialized() const { return initialized; }
  bool has_mesh() const { return has_data; }

private:
  bool initialized = false;
  bool has_data = false;

  SDL_GPUGraphicsPipeline *terrain_pipeline = nullptr;
  SDL_GPUGraphicsPipeline *lava_pipeline = nullptr;
  SDL_GPUGraphicsPipeline *contour_pipeline = nullptr;

  SDL_GPUBuffer *basalt_vbo = nullptr;
  SDL_GPUBuffer *basalt_ibo = nullptr;
  uint32_t basalt_side_index_count = 0;
  uint32_t basalt_total_index_count = 0;

  SDL_GPUBuffer *lava_vbo = nullptr;
  uint32_t lava_vertex_count = 0;

  SDL_GPUBuffer *contour_vbo = nullptr;
  uint32_t contour_vertex_count = 0;

  void release_buffers(SDL_GPUDevice *device);
};
