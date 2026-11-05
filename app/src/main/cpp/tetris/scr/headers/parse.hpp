#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace parser {
    using json = nlohmann::json;

    struct Config
    {
        int rows = 20;
        int cols = 10;
        int gravity_speed_ms = 500;
        int das = 133;
        int arr = 10;
        int sdf = 20;
    };

    Config loadConfig(const std::string &filepath);
}
