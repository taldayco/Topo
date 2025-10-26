#include "contour.h"
#include "noise.h"
#include "render.h"
#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>
#include <vector>

constexpr int MAP_WIDTH = 512;
constexpr int MAP_HEIGHT = 512;
constexpr int WINDOW_WIDTH = 1400;
constexpr int WINDOW_HEIGHT = 1024;

static SDL_Window *window = nullptr;
static SDL_GPUDevice *gpu_device = nullptr;
static TextureHandle map_texture = {};
static std::vector<float> heightmap;
static std::vector<Line> contour_lines;
static NoiseParams noise_params = {0.005f, 6, 2.0f, 0.5f, 1337};
static float contour_interval = 0.05f;
static bool need_regenerate = true;
static bool use_isometric = false;

bool init() {
  SDL_Log("Init starting...");
  if (!SDL_Init(SDL_INIT_VIDEO))
    return false;

  window = SDL_CreateWindow("Topographical Map Generator", WINDOW_WIDTH,
                            WINDOW_HEIGHT, 0);
  if (!window)
    return false;

  gpu_device = SDL_CreateGPUDevice(SDL_GPU_SHADERFORMAT_SPIRV, true, nullptr);
  if (!gpu_device)
    return false;

  if (!SDL_ClaimWindowForGPUDevice(gpu_device, window))
    return false;

  SDL_SetGPUSwapchainParameters(gpu_device, window,
                                SDL_GPU_SWAPCHAINCOMPOSITION_SDR,
                                SDL_GPU_PRESENTMODE_VSYNC);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForVulkan(window);

  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = gpu_device;
  init_info.ColorTargetFormat =
      SDL_GetGPUSwapchainTextureFormat(gpu_device, window);
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;

  ImGui_ImplSDLGPU3_Init(&init_info);

  heightmap.resize(MAP_WIDTH * MAP_HEIGHT);

  SDL_Log("Init complete");
  return true;
}

void regenerate_map() {
  SDL_Log("Starting regeneration...");
  auto start = SDL_GetTicks();

  generate_heightmap(heightmap, MAP_WIDTH, MAP_HEIGHT, noise_params);

  auto after_heightmap = SDL_GetTicks();
  SDL_Log("Heightmap: %llu ms", after_heightmap - start);

  contour_lines.clear();
  extract_contours(heightmap, MAP_WIDTH, MAP_HEIGHT, contour_interval,
                   contour_lines);

  auto after_contours = SDL_GetTicks();
  SDL_Log("Contours (%zu lines): %llu ms", contour_lines.size(),
          after_contours - after_heightmap);

  if (map_texture.texture) {
    release_texture(gpu_device, map_texture);
  }

  SDL_Log("Creating texture with lines...");
  map_texture =
      create_texture_from_heightmap(gpu_device, heightmap, contour_lines,
                                    MAP_WIDTH, MAP_HEIGHT, use_isometric);
  SDL_Log("Texture created");

  SDL_WaitForGPUIdle(gpu_device);

  auto end = SDL_GetTicks();
  SDL_Log("Total regeneration: %llu ms\n", end - start);

  need_regenerate = false;
}

void process_input(bool &running) {
  SDL_Event event;
  while (SDL_PollEvent(&event)) {
    ImGui_ImplSDL3_ProcessEvent(&event);
    if (event.type == SDL_EVENT_QUIT)
      running = false;
  }
}

void render_ui() {
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({1024, 1024}, ImGuiCond_Always);
  ImGui::Begin("Map", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  if (map_texture.texture) {
    ImGui::Image((ImTextureID)map_texture.texture, {1024, 1024});
  } else {
    ImGui::Text("Generating...");
  }

  ImGui::End();

  ImGui::SetNextWindowPos({1024, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({376, 1024}, ImGuiCond_Always);
  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

  ImGui::Text("Noise Parameters");
  need_regenerate |=
      ImGui::SliderFloat("Frequency", &noise_params.frequency, 0.001f, 0.05f);
  need_regenerate |= ImGui::SliderInt("Octaves", &noise_params.octaves, 1, 8);
  need_regenerate |=
      ImGui::SliderFloat("Lacunarity", &noise_params.lacunarity, 1.0f, 4.0f);
  need_regenerate |= ImGui::SliderFloat("Gain", &noise_params.gain, 0.1f, 1.0f);
  need_regenerate |= ImGui::SliderInt("Seed", &noise_params.seed, 0, 10000);

  ImGui::Separator();
  ImGui::Text("Contour Lines");
  need_regenerate |=
      ImGui::SliderFloat("Interval", &contour_interval, 0.05f, 0.2f);
  ImGui::Separator();
  if (ImGui::Button("Regenerate", {-1, 40}))
    need_regenerate = true;
  ImGui::Separator();
  need_regenerate |= ImGui::Checkbox("Isometric View", &use_isometric);
  ImGui::Text("Lines: %zu", contour_lines.size());
  ImGui::Text("Resolution: %dx%d", MAP_WIDTH, MAP_HEIGHT);

  ImGui::End();
  ImGui::Render();
}

void render_frame() {
  SDL_GPUCommandBuffer *cmd = SDL_AcquireGPUCommandBuffer(gpu_device);
  if (!cmd)
    return;

  ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);

  SDL_GPUTexture *swapchain = nullptr;
  if (!SDL_AcquireGPUSwapchainTexture(cmd, window, &swapchain, nullptr,
                                      nullptr) ||
      !swapchain) {
    SDL_SubmitGPUCommandBuffer(cmd);
    return;
  }

  SDL_GPUColorTargetInfo color_target = {};
  color_target.texture = swapchain;
  color_target.clear_color = {0.176f, 0.176f, 0.188f, 1.0f};
  color_target.load_op = SDL_GPU_LOADOP_CLEAR;
  color_target.store_op = SDL_GPU_STOREOP_STORE;

  SDL_GPURenderPass *render_pass =
      SDL_BeginGPURenderPass(cmd, &color_target, 1, nullptr);

  ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, render_pass);

  SDL_EndGPURenderPass(render_pass);
  SDL_SubmitGPUCommandBuffer(cmd);
}

void cleanup() {
  SDL_WaitForGPUIdle(gpu_device);
  if (map_texture.texture) {
    release_texture(gpu_device, map_texture);
  }
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
  if (gpu_device)
    SDL_DestroyGPUDevice(gpu_device);
  if (window)
    SDL_DestroyWindow(window);
  SDL_Quit();
}

int main() {
  SDL_Log("Application starting...");

  if (!init())
    return 1;

  SDL_Log("Entering main loop");
  bool running = true;
  int frame = 0;
  while (running) {
    if (frame % 60 == 0)
      SDL_Log("Frame %d", frame);
    frame++;

    if (need_regenerate)
      regenerate_map();
    process_input(running);
    render_ui();
    render_frame();
  }

  cleanup();
  return 0;
}
