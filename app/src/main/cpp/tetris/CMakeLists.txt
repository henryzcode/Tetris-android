cmake_minimum_required(VERSION 3.22)
project(Ctetris CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


find_package(SDL3 REQUIRED CONFIG COMPONENTS SDL3-shared)
find_package(SDL3_mixer REQUIRED CONFIG)
find_package(SDL3_ttf REQUIRED CONFIG)
find_package(SDL3_image REQUIRED CONFIG)

add_executable(tetris scr/main.cpp scr/game.cpp scr/entities.cpp scr/rules.cpp scr/ui.cpp scr/parse.cpp)

target_compile_options(tetris PRIVATE
        $<$<CONFIG:Release>:-O3>
)

target_link_libraries(tetris PRIVATE
        SDL3::SDL3-shared
        SDL3_mixer::SDL3_mixer
        SDL3_ttf::SDL3_ttf
        SDL3_image::SDL3_image
)

include(FetchContent)
FetchContent_Declare(
    json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
)
FetchContent_MakeAvailable(json)
target_link_libraries(tetris PRIVATE nlohmann_json::nlohmann_json)

add_custom_command(TARGET tetris POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        $<TARGET_FILE_DIR:tetris>/assets
        COMMENT "Copying assets to binary directory..."
)

add_custom_command(TARGET tetris POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/scr/data
        $<TARGET_FILE_DIR:tetris>/data
        COMMENT "Copying data to binary directory..."
)
