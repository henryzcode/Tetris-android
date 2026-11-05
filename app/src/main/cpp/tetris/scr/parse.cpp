#include "headers/parse.hpp"
#include <SDL3/SDL.h>
#include <vector>

namespace parser
{
    Config loadConfig(const std::string &filepath)
    {
        Config config;

        std::vector<std::string> search_paths = {
            filepath,
            "data/" + filepath,
            "assets/data/" + filepath,
            "scr/" + filepath,
            "tetris/" + filepath
        };

        void* buffer = nullptr;
        size_t datasize = 0;
        std::string loaded_path;

        for (const auto& path : search_paths)
        {
            buffer = SDL_LoadFile(path.c_str(), &datasize);
            if (buffer)
            {
                loaded_path = path;
                break;
            }
        }

        if (!buffer)
        {
            SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION,
                        "Could not load config from '%s' (SDL error: %s). Using fallback defaults.",
                        filepath.c_str(), SDL_GetError());
            return config;
        }

        try
        {
            std::string json_str(static_cast<const char*>(buffer), datasize);
            SDL_free(buffer);
            buffer = nullptr;

            json j = json::parse(json_str);

            if (j.contains("grid") && j["grid"].is_object())
            {
                config.rows = j["grid"].value("rows", 20);
                config.cols = j["grid"].value("cols", 10);
            }
            if (j.contains("gameplay") && j["gameplay"].is_object())
            {
                config.gravity_speed_ms = j["gameplay"].value("gravity_speed_ms", 500);
                config.sdf = j["gameplay"].value("sdf", 20);
                config.das = j["gameplay"].value("das", 130);
                config.arr = j["gameplay"].value("arr", 20);
            }

            SDL_LogInfo(SDL_LOG_CATEGORY_APPLICATION,
                        "Successfully loaded config from '%s' (rows=%d, cols=%d, speed=%dms, das=%d, arr=%d, sdf=%d)",
                        loaded_path.c_str(), config.rows, config.cols, config.gravity_speed_ms,
                        config.das, config.arr, config.sdf);

            return config;
        }
        catch (const json::parse_error &e)
        {
            if (buffer) SDL_free(buffer);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "JSON parse error in config '%s': %s. Using default config.",
                         loaded_path.c_str(), e.what());
            return Config{};
        }
        catch (const std::exception &e)
        {
            if (buffer) SDL_free(buffer);
            SDL_LogError(SDL_LOG_CATEGORY_APPLICATION,
                         "Unexpected error parsing config '%s': %s. Using default config.",
                         loaded_path.c_str(), e.what());
            return Config{};
        }
    }
}
