cmake_minimum_required(VERSION 3.20)
project(TopographyGen)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Optimization flags
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# Dependencies
find_package(SDL3 REQUIRED)

include(FetchContent)

FetchContent_Declare(EnTT
    GIT_REPOSITORY https://github.com/skypjack/entt
    GIT_TAG v3.14.0
)
FetchContent_Declare(json
    GIT_REPOSITORY https://github.com/nlohmann/json
    GIT_TAG v3.11.3
)

FetchContent_MakeAvailable(EnTT json)

# ImGui
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
add_library(imgui STATIC
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp
)
target_include_directories(imgui PUBLIC ${IMGUI_DIR} ${IMGUI_DIR}/backends)
target_link_libraries(imgui PUBLIC SDL3::SDL3)

# ---- Shader compilation ----
find_program(GLSLC glslc REQUIRED)

set(SHADER_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src/shaders)
set(SHADER_OUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADER_OUT_DIR})

set(SHADER_SOURCES
    ${SHADER_DIR}/terrain.vert.glsl
    ${SHADER_DIR}/terrain.frag.glsl
    ${SHADER_DIR}/lava.vert.glsl
    ${SHADER_DIR}/lava.frag.glsl
    ${SHADER_DIR}/contour.vert.glsl
    ${SHADER_DIR}/contour.frag.glsl
)

set(SHADER_OUTPUTS)
foreach(SHADER ${SHADER_SOURCES})
    get_filename_component(SHADER_NAME ${SHADER} NAME)
    set(SPV_FILE ${SHADER_OUT_DIR}/${SHADER_NAME}.spv)
    # Extract stage from filename (e.g., terrain.vert.glsl -> vertex)
    string(REGEX MATCH "\\.(vert|frag)\\." STAGE_MATCH ${SHADER_NAME})
    string(REGEX REPLACE "\\." "" SHADER_STAGE ${STAGE_MATCH})
    if(SHADER_STAGE STREQUAL "vert")
        set(STAGE_FLAG "-fshader-stage=vertex")
    else()
        set(STAGE_FLAG "-fshader-stage=fragment")
    endif()
    add_custom_command(
        OUTPUT ${SPV_FILE}
        COMMAND ${GLSLC} ${STAGE_FLAG} -o ${SPV_FILE} ${SHADER}
        DEPENDS ${SHADER}
        COMMENT "Compiling shader ${SHADER_NAME}"
    )
    list(APPEND SHADER_OUTPUTS ${SPV_FILE})
endforeach()

add_custom_target(shaders DEPENDS ${SHADER_OUTPUTS})

# ---- topo_engine (static library) ----
add_library(topo_engine STATIC
    src/engine/app.cpp
    src/engine/gpu/gpu.cpp
    src/engine/input/input.cpp
    src/engine/camera/camera.cpp
    src/engine/ui/imgui_ui.cpp
    src/engine/render/sprite.cpp
    src/engine/render/render_system.cpp
)

target_include_directories(topo_engine PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
)

target_link_libraries(topo_engine PUBLIC
    SDL3::SDL3
    imgui
    EnTT::EnTT
)

# ---- topogen (executable) ----
add_executable(topogen
    src/game/main.cpp
    src/game/topo_game.cpp
    src/game/scenes/menu_scene.cpp
    src/game/scenes/game_scene.cpp
    src/game/scenes/pause_scene.cpp
    src/game/terrain/noise.cpp
    src/game/terrain/contour.cpp
    src/game/terrain/hex.cpp
    src/game/terrain/isometric.cpp
    src/game/terrain/basalt.cpp
    src/game/terrain/lava.cpp
    src/game/terrain/detail.cpp
    src/game/terrain/delve_render.cpp
    src/game/terrain/terrain_generator.cpp
    src/game/terrain/map_gen.cpp
    src/game/terrain/terrain_mesh.cpp
    src/game/terrain/terrain_renderer.cpp
)

target_include_directories(topogen PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/game
    ${CMAKE_CURRENT_SOURCE_DIR}/src/engine
)

target_compile_definitions(topogen PRIVATE
    SHADER_DIR="${SHADER_OUT_DIR}"
)

target_link_libraries(topogen PRIVATE
    topo_engine
    nlohmann_json::nlohmann_json
)

add_dependencies(topogen shaders)
