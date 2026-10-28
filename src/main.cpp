#include "main.h"

#include "spdlog/spdlog.h"
#include "build_info.h"

extern "C" {
int mpv_open_cplugin(mpv_handle *handle) {
    YomitanMPV::Context{handle};
    return 0;
}
}

namespace YomitanMPV {
    Context::Context(mpv_handle *mpv_ctx) {
        this->logger = std::make_shared<spdlog::logger>(PLUGIN_NAME, LOGGER_SINK);
        spdlog::set_level(spdlog::level::debug);
        logger->set_level(spdlog::get_level());

        this->mpv_ctx = mpv_ctx;
        this->util = std::make_shared<Util>(mpv_ctx, logger);
        this->config = std::make_shared<Config>(mpv_ctx, logger, util);

        logger->info("{} ({}) Loaded!", PLUGIN_NAME, PLUGIN_VERSION);
    }
}