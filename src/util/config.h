#pragma once

#include <string>
#include <mpv/client.h>
#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "util.h"

namespace YomitanMPV {
    class Config {
    public:
        struct Keybinds {
            std::string select_mode = "p";

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Keybinds, select_mode);
        };

        struct Configuration {
            std::string yomitan_api_url{"http://127.0.0.1:19633"};
            int scan_length = 20;
            Keybinds keybinds;

            NLOHMANN_DEFINE_TYPE_INTRUSIVE_WITH_DEFAULT(Configuration, yomitan_api_url, scan_length, keybinds);
        };

        Configuration config;

        Config(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<Util> &util);
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;
        std::shared_ptr<Util> util;
    };
}
