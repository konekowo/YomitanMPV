#include "util.h"

namespace YomitanMPV {
    Util::Util(mpv_handle *mpv_handle) {
        this->mpv_ctx = mpv_handle;
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