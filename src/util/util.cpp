#include "util.h"

namespace YomitanMPV {
    Util::Util(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger>& logger) {
        this->mpv_ctx = mpv_ctx;
        this->logger = logger;
    }

    std::string Util::get_mpv_config_path() const {
        mpv_node result;

        const char *cmd[] = {
            "expand-path",
            "~~/",
            nullptr
        };

        if (mpv_command_ret(mpv_ctx, cmd, &result) >= 0) {
            if (result.format == MPV_FORMAT_STRING)
                return result.u.string;

            mpv_free_node_contents(&result);
        }

        return "";
    }

    void Util::osd_show_text(const std::string& text, const std::chrono::milliseconds duration) const {
        const std::string duration_str = std::to_string(duration.count());

        const char *cmd[] = {
            "show-text",
            text.c_str(),
            duration_str.c_str(),
            nullptr
        };

        mpv_command(mpv_ctx, cmd);
    }
}