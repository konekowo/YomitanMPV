#pragma once

#include <mpv/client.h>
#include <spdlog/logger.h>

namespace YomitanMPV {
    class Screen {
    public:
        Screen(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger);
        virtual ~Screen() = default;

        virtual void init() = 0;

        virtual void layout() = 0;

        virtual void poll(const mpv_event *event) = 0;

        virtual void destroy() = 0;

        void set_draw_cb(const std::function<void()> &redraw_cb);
    protected:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;

        std::function<void()> draw = nullptr;
    };
}

