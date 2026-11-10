#include "screen.h"

namespace YomitanMPV {
    Screen::Screen(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger)
        : mpv_ctx(mpv_ctx), logger(logger)
    {
    }

    void Screen::set_draw_cb(const std::function<void()> &draw_cb) {
        draw = draw_cb;
    }
}
