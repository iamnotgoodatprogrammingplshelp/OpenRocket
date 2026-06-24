cmake_minimum_required(VERSION 3.28)
project(OpenOrbit VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Export compile commands for clangd
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Compiler warnings
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Wshadow -Wno-unused-parameter)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Dependencies (vcpkg or system packages)
# ──────────────────────────────────────────────────────────────────────────────
find_package(SFML 2.6 COMPONENTS graphics window system audio QUIET)
if(NOT SFML_FOUND)
    find_package(SFML 2.5 COMPONENTS graphics window system audio REQUIRED)
endif()

find_package(OpenGL REQUIRED)
find_package(Eigen3 3.4 REQUIRED NO_MODULE)
find_package(nlohmann_json 3.10 REQUIRED)
find_package(Catch2 3 REQUIRED)

# Optional: ImGui-SFML for editor
find_package(ImGui-SFML QUIET)

# ──────────────────────────────────────────────────────────────────────────────
# Core simulation library (no graphics dependency — headless-safe)
# ──────────────────────────────────────────────────────────────────────────────
add_library(openorbit_core STATIC
    core/physics/OrbitalMechanics.cpp
    core/physics/Integrators.cpp
    core/physics/PhysicsWorld.cpp
    core/world/CelestialBody.cpp
    core/propulsion/Engine.cpp
    core/aero/Aerodynamics.cpp
    core/thermal/ThermalModel.cpp
    core/parts/PartModule.cpp
)

target_include_directories(openorbit_core
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    PRIVATE ${CMAKE_SOURCE_DIR}/core
)

target_link_libraries(openorbit_core
    PUBLIC  Eigen3::Eigen
    PUBLIC  nlohmann_json::nlohmann_json
)

# ──────────────────────────────────────────────────────────────────────────────
# Renderer library (depends on SFML + OpenGL)
# ──────────────────────────────────────────────────────────────────────────────
add_library(openorbit_renderer STATIC
    renderer/window/AppWindow.cpp
    renderer/sfml_opengl_bridge/SFMLOpenGLBridge.cpp
    renderer/hud/instruments/AltimeterGauge.cpp
    renderer/hud/instruments/AttitudeIndicator.cpp
    renderer/hud/instruments/ThrustGauge.cpp
    renderer/hud/telemetry/TelemetryGraph.cpp
    renderer/hud/map/OrbitalMap.cpp
    renderer/gl3d/PlanetRenderer.cpp
    renderer/gl3d/ShaderProgram.cpp
)

target_include_directories(openorbit_renderer
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}>
    PRIVATE ${CMAKE_SOURCE_DIR}/renderer
)

target_link_libraries(openorbit_renderer
    PUBLIC  openorbit_core
    PUBLIC  sfml-graphics sfml-window sfml-system sfml-audio
    PUBLIC  OpenGL::GL
)

if(ImGui-SFML_FOUND)
    target_link_libraries(openorbit_renderer PUBLIC ImGui-SFML::ImGui-SFML)
    target_compile_definitions(openorbit_renderer PUBLIC OPENORBIT_HAS_IMGUI=1)
endif()

# ──────────────────────────────────────────────────────────────────────────────
# Main executable
# ──────────────────────────────────────────────────────────────────────────────
add_executable(openorbit main.cpp)
target_link_libraries(openorbit PRIVATE openorbit_renderer openorbit_core)

# Copy shaders next to the executable
add_custom_command(TARGET openorbit POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/renderer/shaders
        $<TARGET_FILE_DIR:openorbit>/shaders
    COMMENT "Copying GLSL shaders"
)

# ──────────────────────────────────────────────────────────────────────────────
# Headless simulation binary (Phase 1 validation — no SFML required)
# ──────────────────────────────────────────────────────────────────────────────
add_executable(openorbit_headless headless_main.cpp)
target_link_libraries(openorbit_headless PRIVATE openorbit_core)

# ──────────────────────────────────────────────────────────────────────────────
# Tests
# ──────────────────────────────────────────────────────────────────────────────
enable_testing()
include(CTest)
include(Catch)

add_executable(test_physics
    tests/physics/test_orbital_mechanics.cpp
    tests/physics/test_integrators.cpp
)
target_link_libraries(test_physics PRIVATE openorbit_core Catch2::Catch2WithMain)
catch_discover_tests(test_physics)

add_executable(test_propulsion
    tests/propulsion/test_engine.cpp
)
target_link_libraries(test_propulsion PRIVATE openorbit_core Catch2::Catch2WithMain)
catch_discover_tests(test_propulsion)

add_executable(test_aero
    tests/aero/test_aerodynamics.cpp
)
target_link_libraries(test_aero PRIVATE openorbit_core Catch2::Catch2WithMain)
catch_discover_tests(test_aero)
