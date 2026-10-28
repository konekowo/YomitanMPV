#pragma once

#include <string>
#include <chrono>
#include <mpv/client.h>

namespace YomitanMPV {
    class Util {
    public:
        explicit Util(mpv_handle *mpv_handle);
        void osd_show_text(const std::string& text, std::chrono::milliseconds duration) const;
    private:
        mpv_handle *mpv_ctx;
    };
}
