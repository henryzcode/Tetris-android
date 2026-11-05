cmake_minimum_required(VERSION 3.22.1)
project(ctetris)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ----------------- 1. SDL_image 最小开关 -----------------
set(SDLIMAGE_VENDORED ON CACHE BOOL "" FORCE)
set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_BMP ON CACHE BOOL "" FORCE)
set(SDLIMAGE_GIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JPG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_LBM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PCX OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDLIMAGE_PNM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_QOI OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_SVG OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TGA OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XCF OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XPM OFF CACHE BOOL "" FORCE)
set(SDLIMAGE_XV OFF CACHE BOOL "" FORCE)

# ----------------- 2. SDL_ttf 最小开关 -----------------
set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)

# ----------------- 3. SDL_mixer 最小开关 (仅保留 WAV 和 MP3) -----------------
set(SDLMIXER_VENDORED ON CACHE BOOL "" FORCE)
set(SDLMIXER_CMD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_FLAC OFF CACHE BOOL "" FORCE)
set(SDLMIXER_GME OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MOD OFF CACHE BOOL "" FORCE)
set(SDLMIXER_MP3_MINIMP3 ON CACHE BOOL "" FORCE)
set(SDLMIXER_MIDI OFF CACHE BOOL "" FORCE)
set(SDLMIXER_OPUS OFF CACHE BOOL "" FORCE)
set(SDLMIXER_VORBIS_STB ON CACHE BOOL "" FORCE)
set(SDLMIXER_WAV ON CACHE BOOL "" FORCE)
set(SDLMIXER_WAVPACK OFF CACHE BOOL "" FORCE)

# ----------------- 4. 引入子目录 -----------------
add_subdirectory(SDL)
add_subdirectory(SDL_image)
add_subdirectory(SDL_ttf)
add_subdirectory(SDL_mixer)

# ----------------- 5. 构建主库 -----------------
set(GAME_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tetris/scr)

file(GLOB GAME_SOURCES CONFIGURE_DEPENDS
        ${GAME_SRC_DIR}/*.cpp
        ${GAME_SRC_DIR}/*.c
)

add_library(main SHARED
        ${GAME_SOURCES}
        ${GAME_SRC_DIR}/touch_controller.cpp
)

target_include_directories(main PRIVATE
        ${GAME_SRC_DIR}/headers
        ${GAME_SRC_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL_image/include
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL_ttf/include
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL_mixer/include
)

find_library(log-lib log)
find_library(android-lib android)

target_link_libraries(main PRIVATE
        SDL3::SDL3
        SDL3_image::SDL3_image
        SDL3_ttf::SDL3_ttf
        SDL3_mixer::SDL3_mixer
        ${log-lib}
        ${android-lib}
        GLESv2
        OpenSLES
)