#include "terrain/terrain_renderer.h"
#include <SDL3/SDL.h>
#include <cstring>
#include <string>
#include <vector>

static std::vector<uint8_t> load_shader_file(const char *path) {
  SDL_IOStream *io = SDL_IOFromFile(path, "rb");
  if (!io) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to open shader: %s", path);
    return {};
  }
  Sint64 size = SDL_GetIOSize(io);
  if (size <= 0) {
    SDL_CloseIO(io);
    return {};
  }
  std::vector<uint8_t> data(size);
  SDL_ReadIO(io, data.data(), size);
  SDL_CloseIO(io);
  return data;
}

static SDL_GPUShader *create_shader(SDL_GPUDevice *device, const char *path,
                                    SDL_GPUShaderStage stage,
                                    int num_uniform_buffers) {
  auto code = load_shader_file(path);
  if (code.empty())
    return nullptr;

  SDL_GPUShaderCreateInfo info = {};
  info.code = code.data();
  info.code_size = code.size();
  info.entrypoint = "main";
  info.format = SDL_GPU_SHADERFORMAT_SPIRV;
  info.stage = stage;
  info.num_uniform_buffers = num_uniform_buffers;

  SDL_GPUShader *shader = SDL_CreateGPUShader(device, &info);
  if (!shader) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create shader from %s: %s",
                 path, SDL_GetError());
  }
  return shader;
}

static SDL_GPUBuffer *upload_to_gpu_buffer(SDL_GPUDevice *device,
                                           const void *data, uint32_t size,
                                           SDL_GPUBufferUsageFlags usage) {
  SDL_GPUBufferCreateInfo buf_info = {};
  buf_info.usage = usage;
  buf_info.size = size;

  SDL_GPUBuffer *buffer = SDL_CreateGPUBuffer(device, &buf_info);
  if (!buffer) {
    SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to create GPU buffer");
    return nullptr;
  }

  SDL_GPUTransferBufferCreateInfo transfer_info = {};
  transfer_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
  transfer_info.size = size;

  SDL_GPUTransferBuffer *transfer =
      SDL_CreateGPUTransferBuffer(device, &transfer_info);
  if (!transfer) {
    SDL_ReleaseGPUBuffer(device, buffer);
    return nullptr;
  }

  void *mapped = SDL_MapGPUTransferBuffer(device, transfer, false);
  if (!mapped) {
    SDL_ReleaseGPUTransferBuffer(device, transfer);
    SDL_ReleaseGPUBuffer(device, buffer);
    return nullptr;
  }
  SDL_memcpy(mapped, data, size);
  SDL_UnmapGPUTransferBuffer(device, transfer);

  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(device);
  SDL_GPUCopyPass *copy = SDL_BeginGPUCopyPass(cmd);

  SDL_GPUTransferBufferLocation src = {};
  src.transfer_buffer = transfer;
  src.offset = 0;

  SDL_GPUBufferRegion dst = {};
  dst.buffer = buffer;
  dst.offset = 0;
  dst.size = size;

  SDL_UploadToGPUBuffer(copy, &src, &dst, false);
  SDL_EndGPUCopyPass(copy);
  SDL_SubmitGPUCommandBuffer(cmd);

  SDL_ReleaseGPUTransferBuffer(device, transfer);
  return buffer;
}

void TerrainRenderer::init(SDL_GPUDevice *device, SDL_Window *window) {
  if (initialized)
    return;

  std::string shader_dir = SHADER_DIR;
  SDL_GPUTextureFormat swapchain_format =
      SDL_GetGPUSwapchainTextureFormat(device, window);

  // --- Terrain pipeline (basalt: triangles, opaque) ---
  {
    SDL_GPUShader *vert = create_shader(
        device, (shader_dir + "/terrain.vert.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_VERTEX, 1);
    SDL_GPUShader *frag = create_shader(
        device, (shader_dir + "/terrain.frag.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0);

    if (!vert || !frag) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load terrain shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      return;
    }

    SDL_GPUVertexBufferDescription vbuf_desc = {};
    vbuf_desc.slot = 0;
    vbuf_desc.pitch = sizeof(BasaltVertex);
    vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attrs[2] = {};
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = offsetof(BasaltVertex, pos_x);
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3;
    attrs[1].offset = offsetof(BasaltVertex, color_r);

    SDL_GPUColorTargetDescription color_desc = {};
    color_desc.format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vert;
    pipeline_info.fragment_shader = frag;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vbuf_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 2;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pipeline_info.target_info.color_target_descriptions = &color_desc;
    pipeline_info.target_info.num_color_targets = 1;

    terrain_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!terrain_pipeline) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create terrain pipeline: %s", SDL_GetError());
      return;
    }
  }

  // --- Lava pipeline (triangles, opaque) ---
  {
    SDL_GPUShader *vert = create_shader(
        device, (shader_dir + "/lava.vert.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_VERTEX, 1);
    SDL_GPUShader *frag = create_shader(
        device, (shader_dir + "/lava.frag.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0);

    if (!vert || !frag) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load lava shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      return;
    }

    SDL_GPUVertexBufferDescription vbuf_desc = {};
    vbuf_desc.slot = 0;
    vbuf_desc.pitch = sizeof(GpuLavaVertex);
    vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attrs[3] = {};
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = offsetof(GpuLavaVertex, world_x);
    attrs[1].location = 1;
    attrs[1].buffer_slot = 0;
    attrs[1].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    attrs[1].offset = offsetof(GpuLavaVertex, base_z);
    attrs[2].location = 2;
    attrs[2].buffer_slot = 0;
    attrs[2].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT;
    attrs[2].offset = offsetof(GpuLavaVertex, time_offset);

    SDL_GPUColorTargetDescription color_desc = {};
    color_desc.format = swapchain_format;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vert;
    pipeline_info.fragment_shader = frag;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vbuf_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 3;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_POINTLIST;
    pipeline_info.target_info.color_target_descriptions = &color_desc;
    pipeline_info.target_info.num_color_targets = 1;

    lava_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!lava_pipeline) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create lava pipeline: %s", SDL_GetError());
      return;
    }
  }

  // --- Contour pipeline (lines, alpha blended) ---
  {
    SDL_GPUShader *vert = create_shader(
        device, (shader_dir + "/contour.vert.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_VERTEX, 1);
    SDL_GPUShader *frag = create_shader(
        device, (shader_dir + "/contour.frag.glsl.spv").c_str(),
        SDL_GPU_SHADERSTAGE_FRAGMENT, 0);

    if (!vert || !frag) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Failed to load contour shaders");
      if (vert) SDL_ReleaseGPUShader(device, vert);
      if (frag) SDL_ReleaseGPUShader(device, frag);
      return;
    }

    SDL_GPUVertexBufferDescription vbuf_desc = {};
    vbuf_desc.slot = 0;
    vbuf_desc.pitch = sizeof(ContourVertex);
    vbuf_desc.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    SDL_GPUVertexAttribute attrs[1] = {};
    attrs[0].location = 0;
    attrs[0].buffer_slot = 0;
    attrs[0].format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2;
    attrs[0].offset = 0;

    SDL_GPUColorTargetDescription color_desc = {};
    color_desc.format = swapchain_format;
    color_desc.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    color_desc.blend_state.dst_color_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_desc.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    color_desc.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    color_desc.blend_state.dst_alpha_blendfactor =
        SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    color_desc.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    color_desc.blend_state.enable_blend = true;

    SDL_GPUGraphicsPipelineCreateInfo pipeline_info = {};
    pipeline_info.vertex_shader = vert;
    pipeline_info.fragment_shader = frag;
    pipeline_info.vertex_input_state.vertex_buffer_descriptions = &vbuf_desc;
    pipeline_info.vertex_input_state.num_vertex_buffers = 1;
    pipeline_info.vertex_input_state.vertex_attributes = attrs;
    pipeline_info.vertex_input_state.num_vertex_attributes = 1;
    pipeline_info.primitive_type = SDL_GPU_PRIMITIVETYPE_LINELIST;
    pipeline_info.target_info.color_target_descriptions = &color_desc;
    pipeline_info.target_info.num_color_targets = 1;

    contour_pipeline = SDL_CreateGPUGraphicsPipeline(device, &pipeline_info);
    SDL_ReleaseGPUShader(device, vert);
    SDL_ReleaseGPUShader(device, frag);

    if (!contour_pipeline) {
      SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                   "Failed to create contour pipeline: %s", SDL_GetError());
      return;
    }
  }

  initialized = true;
  SDL_Log("TerrainRenderer: All 3 pipelines created");
}

void TerrainRenderer::release_buffers(SDL_GPUDevice *device) {
  if (basalt_vbo) { SDL_ReleaseGPUBuffer(device, basalt_vbo); basalt_vbo = nullptr; }
  if (basalt_ibo) { SDL_ReleaseGPUBuffer(device, basalt_ibo); basalt_ibo = nullptr; }
  if (lava_vbo) { SDL_ReleaseGPUBuffer(device, lava_vbo); lava_vbo = nullptr; }
  if (contour_vbo) { SDL_ReleaseGPUBuffer(device, contour_vbo); contour_vbo = nullptr; }
  has_data = false;
}

void TerrainRenderer::upload_mesh(SDL_GPUDevice *device,
                                  const TerrainMesh &mesh) {
  SDL_WaitForGPUIdle(device);
  release_buffers(device);

  // Basalt
  if (!mesh.basalt_vertices.empty() && !mesh.basalt_indices.empty()) {
    basalt_vbo = upload_to_gpu_buffer(
        device, mesh.basalt_vertices.data(),
        (uint32_t)(mesh.basalt_vertices.size() * sizeof(BasaltVertex)),
        SDL_GPU_BUFFERUSAGE_VERTEX);

    basalt_ibo = upload_to_gpu_buffer(
        device, mesh.basalt_indices.data(),
        (uint32_t)(mesh.basalt_indices.size() * sizeof(uint32_t)),
        SDL_GPU_BUFFERUSAGE_INDEX);

    basalt_side_index_count = mesh.side_index_count;
    basalt_total_index_count = (uint32_t)mesh.basalt_indices.size();
  }

  // Lava
  if (!mesh.lava_vertices.empty()) {
    lava_vbo = upload_to_gpu_buffer(
        device, mesh.lava_vertices.data(),
        (uint32_t)(mesh.lava_vertices.size() * sizeof(GpuLavaVertex)),
        SDL_GPU_BUFFERUSAGE_VERTEX);
    lava_vertex_count = (uint32_t)mesh.lava_vertices.size();
  }

  // Contour
  if (!mesh.contour_vertices.empty()) {
    contour_vbo = upload_to_gpu_buffer(
        device, mesh.contour_vertices.data(),
        (uint32_t)(mesh.contour_vertices.size() * sizeof(ContourVertex)),
        SDL_GPU_BUFFERUSAGE_VERTEX);
    contour_vertex_count = (uint32_t)mesh.contour_vertices.size();
  }

  has_data = true;
  SDL_Log("TerrainRenderer: Mesh uploaded (basalt=%u idx, lava=%u verts, contour=%u verts)",
          basalt_total_index_count, lava_vertex_count, contour_vertex_count);
}

void TerrainRenderer::draw(SDL_GPURenderPass *render_pass,
                           SDL_GPUCommandBuffer *cmd,
                           const SceneUniforms &uniforms) {
  if (!initialized || !has_data)
    return;

  // Draw order: sides -> lava -> tops -> contours

  // 1. Basalt side faces
  if (basalt_vbo && basalt_ibo && basalt_side_index_count > 0) {
    SDL_BindGPUGraphicsPipeline(render_pass, terrain_pipeline);
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vbuf_bind = {};
    vbuf_bind.buffer = basalt_vbo;
    vbuf_bind.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vbuf_bind, 1);

    SDL_GPUBufferBinding ibuf_bind = {};
    ibuf_bind.buffer = basalt_ibo;
    ibuf_bind.offset = 0;
    SDL_BindGPUIndexBuffer(render_pass, &ibuf_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    SDL_DrawGPUIndexedPrimitives(render_pass, basalt_side_index_count, 1, 0, 0, 0);
  }

  // 2. Lava
  if (lava_vbo && lava_vertex_count > 0) {
    SDL_BindGPUGraphicsPipeline(render_pass, lava_pipeline);
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vbuf_bind = {};
    vbuf_bind.buffer = lava_vbo;
    vbuf_bind.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vbuf_bind, 1);

    SDL_DrawGPUPrimitives(render_pass, lava_vertex_count, 1, 0, 0);
  }

  // 3. Basalt top faces
  if (basalt_vbo && basalt_ibo &&
      basalt_total_index_count > basalt_side_index_count) {
    SDL_BindGPUGraphicsPipeline(render_pass, terrain_pipeline);
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vbuf_bind = {};
    vbuf_bind.buffer = basalt_vbo;
    vbuf_bind.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vbuf_bind, 1);

    SDL_GPUBufferBinding ibuf_bind = {};
    ibuf_bind.buffer = basalt_ibo;
    ibuf_bind.offset = 0;
    SDL_BindGPUIndexBuffer(render_pass, &ibuf_bind,
                           SDL_GPU_INDEXELEMENTSIZE_32BIT);

    uint32_t top_index_count =
        basalt_total_index_count - basalt_side_index_count;
    SDL_DrawGPUIndexedPrimitives(render_pass, top_index_count, 1,
                                 basalt_side_index_count, 0, 0);
  }

  // 4. Contour lines
  if (contour_vbo && contour_vertex_count > 0) {
    SDL_BindGPUGraphicsPipeline(render_pass, contour_pipeline);
    SDL_PushGPUVertexUniformData(cmd, 0, &uniforms, sizeof(uniforms));

    SDL_GPUBufferBinding vbuf_bind = {};
    vbuf_bind.buffer = contour_vbo;
    vbuf_bind.offset = 0;
    SDL_BindGPUVertexBuffers(render_pass, 0, &vbuf_bind, 1);

    SDL_DrawGPUPrimitives(render_pass, contour_vertex_count, 1, 0, 0);
  }
}

void TerrainRenderer::cleanup(SDL_GPUDevice *device) {
  release_buffers(device);

  if (terrain_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device, terrain_pipeline);
    terrain_pipeline = nullptr;
  }
  if (lava_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device, lava_pipeline);
    lava_pipeline = nullptr;
  }
  if (contour_pipeline) {
    SDL_ReleaseGPUGraphicsPipeline(device, contour_pipeline);
    contour_pipeline = nullptr;
  }

  initialized = false;
  SDL_Log("TerrainRenderer: Cleaned up");
}
