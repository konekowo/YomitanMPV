#pragma once

#include <mpv/client.h>
#include <spdlog/logger.h>

#include "clay_ext.h"
#include "util/util.h"

namespace YomitanMPV {
    class Screen {
    public:
        Screen(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<Util> &util);
        virtual ~Screen() = default;

        void register_input(const std::string &lock_name);

        virtual void init() = 0;

        virtual void layout() = 0;

        virtual void poll(const mpv_event *event) = 0;

        virtual void destroy() = 0;

        void set_draw_cb(const std::function<void()> &draw_cb);

        void set_width(int64_t width);
        void set_height(int64_t height);
        void set_scale(float scale);
    protected:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;
        std::shared_ptr<Util> util;

        std::function<void()> draw = nullptr;

        std::shared_ptr<TextConfigExt> default_text_config_ext = std::make_shared<TextConfigExt>();

        int64_t width = 1920;
        int64_t height = 1080;
        float scale = 1.0f;

        bool mouse_down = false;
    };
}

