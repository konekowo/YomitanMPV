#pragma once
#include <map>
#include <memory>
#include <mpv/client.h>
#include <spdlog/logger.h>

#include "clay.h"
#include "screen.h"

namespace YomitanMPV {
    class Display {
    public:
        Display(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger>& logger);
        ~Display();

        void poll(const mpv_event *event);

        void draw();

        void set_current_screen(const std::shared_ptr<Screen>& screen);
        std::shared_ptr<Screen> get_current_screen();
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;

        void *clayArenaMemory;
        Clay_Arena clayArena;

        int64_t last_width = 1920;
        int64_t last_height = 1080;
        int64_t width = 1920;
        int64_t height = 1080;
        float scale = 1.0f;

        bool ui_layer_drawn = false;

        std::shared_ptr<Screen> current_screen = nullptr;

        std::map<uint16_t, std::string> font_id_map;

        void handle_error(const Clay_ErrorType &errorType, const Clay_String &errorText) const;

        Clay_Dimensions measure_text(const Clay_StringSlice &text, Clay_TextElementConfig *config) const;

        struct MPVBounds {
            double x0;
            double y0;
            double x1;
            double y1;

            [[nodiscard]] double width() const {
                return x1 - x0;
            }
            [[nodiscard]] double height() const {
                return y1 - y0;
            }
        };

        MPVBounds get_bounds(const std::string &ass_event) const;
    };
}
