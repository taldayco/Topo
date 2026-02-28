#pragma once
#include "terrain/terrain_mesh.h"
#include <SDL3/SDL.h>
#include <vector>

class TerrainRenderer {
public:
  void init(SDL_GPUDevice *device, SDL_Window *window);
  void upload_mesh(SDL_GPUDevice *device, const TerrainMesh &mesh);



  void draw(SDL_GPUCommandBuffer *cmd,
            SDL_GPUTexture *swapchain,
            uint32_t w, uint32_t h,
            const SceneUniforms &uniforms,
            const std::vector<GpuPointLight> &lights);




  void rebuild_clusters_if_needed(SDL_GPUCommandBuffer *cmd,
                                   uint32_t w, uint32_t h,
                                   float tile_px, uint32_t num_slices,
                                   float near_plane, float far_plane);


  SDL_GPURenderPass *begin_render_pass(SDL_GPUCommandBuffer *cmd,
                                       SDL_GPUTexture *swapchain,
                                       uint32_t w, uint32_t h);

  SDL_GPURenderPass *begin_render_pass_load(SDL_GPUCommandBuffer *cmd,
                                            SDL_GPUTexture *swapchain,
                                            uint32_t w, uint32_t h);

  void cleanup(SDL_GPUDevice *device);

  bool is_initialized() const { return initialized; }
  bool has_mesh()       const { return has_data; }
  SDL_GPUTextureFormat get_depth_format() const { return depth_stencil_format; }

  uint32_t cluster_tiles_x() const { return cluster_grid_w; }
  uint32_t cluster_tiles_y() const { return cluster_grid_y; }

private:

  void init_graphics_pipelines(SDL_GPUDevice *device, SDL_Window *window);
  void init_compute_pipelines(SDL_GPUDevice *device);
  void init_cluster_buffers(SDL_GPUDevice *device, uint32_t tilesX, uint32_t tilesY, uint32_t num_slices);


  void stage_geometry(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd,
                      const SceneUniforms &uniforms);
  void stage_cull_lights(SDL_GPUCommandBuffer *cmd,
                         const SceneUniforms &uniforms,
                         const std::vector<GpuPointLight> &lights);
  void stage_shaded_draw(SDL_GPURenderPass *pass, SDL_GPUCommandBuffer *cmd,
                         const SceneUniforms &uniforms);


  void release_buffers(SDL_GPUDevice *device);
  void release_cluster_buffers(SDL_GPUDevice *device);
  void upload_lights(const std::vector<GpuPointLight> &lights);


  bool initialized = false;
  bool has_data    = false;

  SDL_GPUDevice           *gpu_device           = nullptr;
  SDL_GPUTextureFormat     depth_stencil_format  = SDL_GPU_TEXTUREFORMAT_D32_FLOAT_S8_UINT;
  SDL_GPUTexture          *depth_texture         = nullptr;
  uint32_t                 depth_w = 0, depth_h  = 0;


  SDL_GPUGraphicsPipeline *terrain_pipeline         = nullptr;
  SDL_GPUGraphicsPipeline *terrain_stencil_pipeline = nullptr;
  SDL_GPUGraphicsPipeline *lava_pipeline            = nullptr;
  SDL_GPUGraphicsPipeline *contour_pipeline         = nullptr;


  SDL_GPUComputePipeline  *cluster_gen_pipeline     = nullptr;
  SDL_GPUComputePipeline  *light_culling_pipeline   = nullptr;


  SDL_GPUBuffer *basalt_vbo = nullptr;
  SDL_GPUBuffer *basalt_ibo = nullptr;
  uint32_t       basalt_side_index_count  = 0;
  uint32_t       basalt_total_index_count = 0;

  SDL_GPUBuffer *lava_vbo       = nullptr;
  SDL_GPUBuffer *lava_ibo       = nullptr;
  uint32_t       lava_vertex_count = 0;
  uint32_t       lava_index_count  = 0;

  SDL_GPUBuffer *void_vbo       = nullptr;
  uint32_t       void_vertex_count = 0;

  SDL_GPUBuffer *contour_vbo    = nullptr;
  uint32_t       contour_vertex_count = 0;


  SDL_GPUBuffer *point_light_ssbo   = nullptr;
  SDL_GPUBuffer *cluster_aabb_ssbo  = nullptr;
  SDL_GPUBuffer *light_grid_ssbo    = nullptr;
  SDL_GPUBuffer *global_index_ssbo  = nullptr;
  SDL_GPUBuffer *cull_counter_ssbo  = nullptr;



  SDL_GPUTransferBuffer *counter_reset_transfer = nullptr;


  uint32_t cluster_grid_w = 0;
  uint32_t cluster_grid_y = 0;


  uint32_t current_light_count = 0;


  static constexpr uint32_t MAX_LIGHTS        = 1024;
  static constexpr uint32_t MAX_LIGHT_INDICES = 65536;
};
