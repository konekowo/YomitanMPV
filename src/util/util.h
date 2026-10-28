#pragma once

#include <string>
#include <chrono>
#include <memory>
#include <mpv/client.h>
#include <spdlog/logger.h>

namespace YomitanMPV {
    class Util {
    public:
        explicit Util(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger>& logger);
        [[nodiscard]] std::string get_mpv_config_path() const;
        void osd_show_text(const std::string& text, std::chrono::milliseconds duration) const;
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;
    };
}
