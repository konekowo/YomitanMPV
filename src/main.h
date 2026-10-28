#pragma once

#include <memory>
#include <mpv/client.h>

#include <spdlog/sinks/ansicolor_sink.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "util/util.h"

namespace YomitanMPV {
    class Context {
    public:
        explicit Context(mpv_handle *mpv_ctx);
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<Util> util;
    };
}

const inline std::initializer_list<std::shared_ptr<spdlog::sinks::sink>> LOGGER_SINK = {
    std::make_shared<spdlog::sinks::stdout_color_sink_mt>()
};
