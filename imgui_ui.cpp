#include "imgui_ui.h"
#include "palettes.h"
#include <algorithm>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlgpu3.h>

static void clamp_pan(ViewState &v) {
    float half = 0.5f / v.zoom;
    v.pan_x = std::clamp(v.pan_x, half, 1.0f - half);
    v.pan_y = std::clamp(v.pan_y, half, 1.0f - half);
}

static void compute_uvs(ViewState &v, ImVec2 &uv0, ImVec2 &uv1) {
    clamp_pan(v);
    float half = 0.5f / v.zoom;
    uv0 = {v.pan_x - half, v.pan_y - half};
    uv1 = {v.pan_x + half, v.pan_y + half};
}

void ui_init(SDL_Window *window, SDL_GPUDevice *device) {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO &io = ImGui::GetIO();
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

  ImGui::StyleColorsDark();
  ImGui_ImplSDL3_InitForVulkan(window);

  ImGui_ImplSDLGPU3_InitInfo init_info = {};
  init_info.Device = device;
  init_info.ColorTargetFormat =
      SDL_GetGPUSwapchainTextureFormat(device, window);
  init_info.SwapchainComposition = SDL_GPU_SWAPCHAINCOMPOSITION_SDR;
  init_info.PresentMode = SDL_GPU_PRESENTMODE_VSYNC;

  ImGui_ImplSDLGPU3_Init(&init_info);
}

void ui_shutdown() {
  ImGui_ImplSDLGPU3_Shutdown();
  ImGui_ImplSDL3_Shutdown();
  ImGui::DestroyContext();
}

void ui_process_event(SDL_Event &event) {
  ImGui_ImplSDL3_ProcessEvent(&event);
}

void ui_prepare_draw(SDL_GPUCommandBuffer *cmd) {
  ImGui_ImplSDLGPU3_PrepareDrawData(ImGui::GetDrawData(), cmd);
}

void ui_draw(SDL_GPUCommandBuffer *cmd, SDL_GPURenderPass *render_pass) {
  ImGui_ImplSDLGPU3_RenderDrawData(ImGui::GetDrawData(), cmd, render_pass);
}

void ui_render(AppState &state, const TextureHandle &map_texture) {
  ImGui_ImplSDLGPU3_NewFrame();
  ImGui_ImplSDL3_NewFrame();
  ImGui::NewFrame();

  ImGui::SetNextWindowPos({50, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      {(float)Config::WINDOW_HEIGHT, (float)Config::WINDOW_HEIGHT},
      ImGuiCond_Always);
  ImGui::Begin("Map", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse);

  if (map_texture.texture) {
    float tex_w = map_texture.width;
    float tex_h = map_texture.height;
    float window_w = (float)Config::WINDOW_HEIGHT;
    float window_h = (float)Config::WINDOW_HEIGHT;

    float scale = std::min(window_w / tex_w, window_h / tex_h);
    float display_w = tex_w * scale;
    float display_h = tex_h * scale;

    float offset_x = (window_w - display_w) * 0.5f;
    float offset_y = (window_h - display_h) * 0.5f;

    ImGui::SetCursorPos({offset_x, offset_y});
    ImVec2 uv0, uv1;
    compute_uvs(state.view, uv0, uv1);
    ImGui::Image((ImTextureID)map_texture.texture, {display_w, display_h}, uv0, uv1);

    // Keyboard zoom (toward view center)
    float zoom_input = 0.0f;
    if (ImGui::IsKeyPressed(ImGuiKey_Equal))  zoom_input =  1.0f;  // + key
    if (ImGui::IsKeyPressed(ImGuiKey_Minus))  zoom_input = -1.0f;  // - key
    if (zoom_input != 0.0f) {
      state.view.zoom *= 1.0f + zoom_input * 0.12f;
      state.view.zoom = std::clamp(state.view.zoom, 1.0f, 16.0f);
      clamp_pan(state.view);
    }

    // Mouse drag pan
    if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
      ImVec2 delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
      ImGui::ResetMouseDragDelta(ImGuiMouseButton_Left);
      state.view.pan_x -= delta.x / display_w / state.view.zoom;
      state.view.pan_y -= delta.y / display_h / state.view.zoom;
      clamp_pan(state.view);
    }
  } else {
    ImGui::Text("Generating...");
  }

  ImGui::End();

  ImGui::SetNextWindowPos({(float)Config::WINDOW_HEIGHT, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize(
      {(float)Config::UI_PANEL_WIDTH, (float)Config::WINDOW_HEIGHT},
      ImGuiCond_Always);
  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

  ImGui::Text("Noise Parameters");
  state.need_regenerate |= ImGui::SliderFloat(
      "Frequency", &state.noise_params.frequency, 0.001f, 0.05f);
  state.need_regenerate |=
      ImGui::SliderInt("Octaves", &state.noise_params.octaves, 1, 8);
  state.need_regenerate |= ImGui::SliderFloat(
      "Lacunarity", &state.noise_params.lacunarity, 1.0f, 4.0f);
  state.need_regenerate |=
      ImGui::SliderFloat("Gain", &state.noise_params.gain, 0.1f, 1.0f);
  state.need_regenerate |=
      ImGui::SliderInt("Seed", &state.noise_params.seed, 0, 10000);
  state.need_regenerate |= ImGui::SliderInt(
      "Terrace Levels", &state.noise_params.terrace_levels, 3, 20);
  state.need_regenerate |= ImGui::SliderInt(
      "Min Region Size", &state.noise_params.min_region_size, 50, 2000);
  ImGui::Separator();
  ImGui::Text("Contour Lines");
  state.need_regenerate |=
      ImGui::SliderFloat("Interval", &state.contour_interval, 0.05f, 0.2f);

  ImGui::Separator();
  ImGui::Text("Color Palette");
  if (ImGui::BeginCombo("##palette",
                         PALETTES[state.current_palette].name)) {
    for (int i = 0; i < PALETTE_COUNT; ++i) {
      bool is_selected = (state.current_palette == i);
      if (ImGui::Selectable(PALETTES[i].name, is_selected)) {
        state.current_palette = i;
        state.need_regenerate = true;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  state.need_regenerate |=
      ImGui::Checkbox("Isometric View", &state.use_isometric);
  if (state.use_isometric) {
    state.need_regenerate |=
        ImGui::SliderFloat("Padding", &state.iso_padding, 0.0f, 200.0f);
    state.need_regenerate |= ImGui::SliderFloat(
        "Offset X", &state.iso_offset_x_adjust, -200.0f, 200.0f);
    state.need_regenerate |= ImGui::SliderFloat(
        "Offset Y", &state.iso_offset_y_adjust, -200.0f, 200.0f);
  }

  ImGui::Separator();
  ImGui::Text("Map Scale");
  state.need_regenerate |=
      ImGui::SliderFloat("Map Scale", &state.map_scale, 0.25f, 4.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Controls zoom level: higher = more terrain visible");
  }

  ImGui::Separator();
  if (ImGui::Button("Regenerate", {175, 40}))
    state.need_regenerate = true;
  ImGui::SameLine();
  if (ImGui::Button("Reset", {175, 40})) {
    state.noise_params = DEFAULT_NOISE;
    state.contour_interval = Config::DEFAULT_CONTOUR_INTERVAL;
    state.use_isometric = DEFAULT_ISOMETRIC;
    state.current_palette = 0;
    state.map_scale = Config::DEFAULT_MAP_SCALE;
    state.view = ViewState{};
    state.need_regenerate = true;
  }
  if (ImGui::Button("Reset View", {175, 40})) {
    state.view = ViewState{};
  }

  ImGui::Separator();
  ImGui::Text("Stats");
  ImGui::Text("Lines: %zu", state.contour_lines.size());
  ImGui::Text("Resolution: %dx%d", Config::MAP_WIDTH, Config::MAP_HEIGHT);
  ImGui::Text("View Zoom: %.1fx", state.view.zoom);

  ImGui::End();
  ImGui::Render();
}
