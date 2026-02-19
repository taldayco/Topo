#include "topo_game.h"
#include "scenes/game_scene.h"
#include "scenes/menu_scene.h"
#include "scenes/pause_scene.h"
#include "terrain/map_gen.h"
#include "terrain/noise.h"
#include "terrain/contour.h"
#include "terrain/palettes.h"
#include "ui/imgui_ui.h"
#include <imgui.h>

std::unique_ptr<Scene> TopoGame::create_scene(SceneID id) {
  switch (id) {
  case SceneID::Menu:  return std::make_unique<MenuScene>(*this);
  case SceneID::Game:  return std::make_unique<GameScene>(*this);
  case SceneID::Pause: return std::make_unique<PauseScene>(*this);
  default: return nullptr;
  }
}

void TopoGame::push_scene(SceneID id) {
  auto scene = create_scene(id);
  if (scene)
    scenes.push(std::move(scene), gpu_ctx);
}

void TopoGame::pop_scene() {
  scenes.pop();
}

void TopoGame::switch_scene(SceneID id) {
  auto scene = create_scene(id);
  if (scene)
    scenes.switch_to(std::move(scene), gpu_ctx);
}

void TopoGame::on_init(GpuContext &gpu) {
  push_scene(SceneID::Game);
}

void TopoGame::on_event(const SDL_Event &event) {
  scenes.handle_event(event);
}

void TopoGame::on_update(float dt) {
  scenes.update(dt);
}

void TopoGame::on_render_tool(GpuContext &gpu, FrameContext &frame) {
  render_ui(gpu.game_window != nullptr);
  ui_prepare_draw(frame.cmd);
  gpu_begin_render_pass(gpu, frame);
  ui_draw(frame.cmd, frame.render_pass);
}

void TopoGame::on_render_game(GpuContext &gpu, FrameContext &frame) {
  // Lazy-init the renderer on first game frame
  if (!terrain_renderer.is_initialized()) {
    terrain_renderer.init(gpu.device, gpu.game_window);
  }

  // Regenerate terrain mesh when needed
  if (app_state.need_regenerate) {
    SDL_Log("Starting GPU regeneration...");
    auto start = SDL_GetTicks();

    app_state.heightmap.resize(Config::MAP_WIDTH * Config::MAP_HEIGHT);
    generate_heightmap(app_state.heightmap, Config::MAP_WIDTH,
                       Config::MAP_HEIGHT, app_state.noise_params,
                       app_state.map_scale);

    app_state.contour_lines.clear();
    extract_contours(app_state.heightmap, Config::MAP_WIDTH,
                     Config::MAP_HEIGHT, app_state.contour_interval,
                     app_state.contour_lines, app_state.band_map);

    terrain_mesh = build_terrain_mesh(app_state);
    terrain_renderer.upload_mesh(gpu.device, terrain_mesh);

    app_state.need_regenerate = false;
    SDL_Log("GPU regeneration: %llu ms", SDL_GetTicks() - start);
  }

  // Begin render pass and draw terrain
  gpu_begin_render_pass(gpu, frame);

  if (terrain_renderer.has_mesh()) {
    float time = SDL_GetTicks() / 1000.0f;
    SceneUniforms uniforms = compute_uniforms(
        terrain_mesh, app_state.view, frame.swapchain_w, frame.swapchain_h,
        time, app_state.contour_opacity);
    terrain_renderer.draw(frame.render_pass, frame.cmd, uniforms);
  }
}

void TopoGame::on_cleanup() {
  scenes.cleanup();
  terrain_renderer.cleanup(gpu_ctx.device);
}

bool TopoGame::wants_game_window_open() {
  if (app_state.launch_game_requested) {
    app_state.launch_game_requested = false;
    return true;
  }
  return false;
}

bool TopoGame::wants_game_window_close() {
  if (app_state.close_game_requested) {
    app_state.close_game_requested = false;
    return true;
  }
  return false;
}

void TopoGame::render_ui(bool game_window_open) {
  ui_begin_frame();

  ImGuiIO &io = ImGui::GetIO();
  ImGui::SetNextWindowPos({0, 0}, ImGuiCond_Always);
  ImGui::SetNextWindowSize({io.DisplaySize.x, io.DisplaySize.y},
                           ImGuiCond_Always);
  ImGui::Begin("Controls", nullptr,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

  if (!game_window_open) {
    if (ImGui::Button("Launch Game", {-1, 40}))
      app_state.launch_game_requested = true;
  } else {
    if (ImGui::Button("Close Game", {-1, 40}))
      app_state.close_game_requested = true;
  }

  ImGui::Separator();
  ImGui::Text("Noise Parameters");
  app_state.need_regenerate |= ImGui::SliderFloat(
      "Frequency", &app_state.noise_params.frequency, 0.001f, 0.05f);
  app_state.need_regenerate |=
      ImGui::SliderInt("Octaves", &app_state.noise_params.octaves, 1, 8);
  app_state.need_regenerate |= ImGui::SliderFloat(
      "Lacunarity", &app_state.noise_params.lacunarity, 1.0f, 4.0f);
  app_state.need_regenerate |=
      ImGui::SliderFloat("Gain", &app_state.noise_params.gain, 0.1f, 1.0f);
  app_state.need_regenerate |=
      ImGui::SliderInt("Seed", &app_state.noise_params.seed, 0, 10000);
  app_state.need_regenerate |= ImGui::SliderInt(
      "Terrace Levels", &app_state.noise_params.terrace_levels, 3, 20);
  app_state.need_regenerate |= ImGui::SliderInt(
      "Min Region Size", &app_state.noise_params.min_region_size, 50, 2000);
  ImGui::Separator();
  ImGui::Text("Contour Lines");
  app_state.need_regenerate |=
      ImGui::SliderFloat("Interval", &app_state.contour_interval, 0.05f, 0.2f);

  ImGui::Separator();
  ImGui::Text("Color Palette");
  if (ImGui::BeginCombo("##palette",
                         PALETTES[app_state.current_palette].name)) {
    for (int i = 0; i < PALETTE_COUNT; ++i) {
      bool is_selected = (app_state.current_palette == i);
      if (ImGui::Selectable(PALETTES[i].name, is_selected)) {
        app_state.current_palette = i;
        app_state.need_regenerate = true;
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Separator();
  app_state.need_regenerate |=
      ImGui::Checkbox("Isometric View", &app_state.use_isometric);
  if (app_state.use_isometric) {
    app_state.need_regenerate |=
        ImGui::SliderFloat("Padding", &app_state.iso_padding, 0.0f, 200.0f);
    app_state.need_regenerate |= ImGui::SliderFloat(
        "Offset X", &app_state.iso_offset_x_adjust, -200.0f, 200.0f);
    app_state.need_regenerate |= ImGui::SliderFloat(
        "Offset Y", &app_state.iso_offset_y_adjust, -200.0f, 200.0f);
  }

  ImGui::Separator();
  ImGui::Text("Map Scale");
  app_state.need_regenerate |=
      ImGui::SliderFloat("Map Scale", &app_state.map_scale, 0.25f, 4.0f);
  if (ImGui::IsItemHovered()) {
    ImGui::SetTooltip("Controls zoom level: higher = more terrain visible");
  }

  ImGui::Separator();
  if (ImGui::Button("Regenerate", {-1, 40}))
    app_state.need_regenerate = true;
  if (ImGui::Button("Reset", {-1, 40})) {
    app_state.noise_params = DEFAULT_NOISE;
    app_state.contour_interval = Config::DEFAULT_CONTOUR_INTERVAL;
    app_state.use_isometric = DEFAULT_ISOMETRIC;
    app_state.current_palette = 0;
    app_state.map_scale = Config::DEFAULT_MAP_SCALE;
    app_state.view = ViewState{};
    app_state.need_regenerate = true;
  }

  ImGui::Separator();
  ImGui::Text("Stats");
  ImGui::Text("Lines: %zu", app_state.contour_lines.size());
  ImGui::Text("Resolution: %dx%d", Config::MAP_WIDTH, Config::MAP_HEIGHT);
  ImGui::Text("View Zoom: %.1fx", app_state.view.zoom);

  ImGui::End();

  ui_end_frame();
}
