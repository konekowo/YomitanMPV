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
        this->mpv_ctx = mpv_ctx;
        this->util = std::make_shared<Util>(mpv_ctx);

        spdlog::set_level(spdlog::level::debug);

        const auto logger = std::make_shared<spdlog::logger>(PLUGIN_NAME, LOGGER_SINK);
        logger->set_level(spdlog::get_level());

        logger->info("{} ({}) Loaded!", PLUGIN_NAME, PLUGIN_VERSION);
        
    }
}