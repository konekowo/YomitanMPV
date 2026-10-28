#pragma once

#include <string>
#include <mpv/client.h>
#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "util.h"

namespace YomitanMPV {
    class Config {
    public:
        Config(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<Util> &util);

        std::string yomitan_api_url{"http://127.0.0.1:19633"};

        NLOHMANN_DEFINE_TYPE_INTRUSIVE(Config, yomitan_api_url);
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;
        std::shared_ptr<Util> util;
    };
}
