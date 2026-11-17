#pragma once
#include <map>
#include <memory>
#include <string>
#include <mpv/client.h>
#include <spdlog/logger.h>

#include "clay.h"
#include "screen.h"

namespace YomitanMPV {
    class FontMeasure;

    class Display {
    public:
        Display(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger>& logger);
        ~Display();

        void poll(const mpv_event *event);

        void draw(bool force_draw);

        void set_current_screen(const std::shared_ptr<Screen>& screen);
        std::shared_ptr<Screen> get_current_screen();
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;

        void *clayArenaMemory;
        Clay_Arena clayArena;
        Clay_Context *clayContext;

        int64_t last_width = 1920;
        int64_t last_height = 1080;
        int64_t width = 1920;
        int64_t height = 1080;
        float scale = 1.0f;

        bool ui_layer_drawn = false;

        std::shared_ptr<Screen> current_screen = nullptr;

        std::map<uint16_t, std::string> font_id_map;
        std::string default_font = "sans-serif";

        std::unique_ptr<FontMeasure> text_measurer;

        void handle_error(const Clay_ErrorType &errorType, const Clay_String &errorText) const;

        Clay_Dimensions measure_text(const Clay_StringSlice &text, Clay_TextElementConfig *config) const;

        const std::string &font_family(uint16_t font_id) const;
    };
}
