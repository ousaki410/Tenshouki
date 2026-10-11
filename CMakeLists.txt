cmake_minimum_required(VERSION 3.20)
project(tensyoki VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Find required packages
find_package(SDL3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(DirectXShaderCompiler CONFIG REQUIRED)

# Fetch Dear ImGui
include(FetchContent)
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.8
)
FetchContent_MakeAvailable(imgui)

add_executable(tensyoki)
target_sources(tensyoki PRIVATE
    src/main.cpp
    src/core/config.cpp
    src/core/filesystem.cpp
    src/core/logging.cpp
    src/resource/bin_parser.cpp
    src/resource/grp_parser.cpp
    src/resource/dat_parser.cpp
    src/ui/imgui_backend.cpp
    src/platform/platform_linux.cpp
)

target_include_directories(tensyoki PRIVATE
    src/core
    src/resource
    src/ui
    src/platform
    src/render
    src/test
)

target_link_libraries(tensyoki PRIVATE
    SDL3::SDL3
    ImGui::imgui
    spdlog::spdlog
    fmt::fmt
    DXC::dxcompiler
)
