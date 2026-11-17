#pragma once

#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <vector>
#include <spdlog/logger.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "screens/clay.h"

namespace YomitanMPV {
    class FontMeasure {
    public:
        explicit FontMeasure(const std::shared_ptr<spdlog::logger> &logger);
        ~FontMeasure();

        FontMeasure(const FontMeasure &) = delete;
        FontMeasure &operator=(const FontMeasure &) = delete;

        Clay_Dimensions measure(const Clay_StringSlice &text, const Clay_TextElementConfig &config, const std::string &family);

        // Use this name in ASS output so native font aliases select the measured face.
        [[nodiscard]] std::string render_font_name(const std::string &family);

        void clear_cache();
    private:
        struct Font;
        using FontKey = std::tuple<std::filesystem::path, FT_Long, std::string>;

        std::shared_ptr<spdlog::logger> logger;
        FT_Library library = nullptr;

        std::map<FontKey, std::unique_ptr<Font>> fonts;
        std::map<std::pair<std::string, uint32_t>, Font *> font_requests;
        std::map<std::string, std::vector<Font *>> fallback_fonts;

        Font *load_font(const std::string &family, uint32_t codepoint = 0);
        Font *font_for_character(const std::string &family, Font *base, uint32_t character);
    };
}
