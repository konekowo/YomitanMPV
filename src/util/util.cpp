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

    std::string Util::get_subtitle_language(const int64_t sid) const {
        int64_t count = 0;

        if (mpv_get_property(mpv_ctx, "track-list/count", MPV_FORMAT_INT64, &count) < 0)
            return "";

        for (int64_t i = 0; i < count; i++) {
            char *type = mpv_get_property_string(mpv_ctx, ("track-list/" + std::to_string(i) + "/type").c_str());

            const bool is_sub = type && std::string(type) == "sub";
            mpv_free(type);

            if (!is_sub)
                continue;

            int64_t id = 0;

            if (mpv_get_property(mpv_ctx, ("track-list/" + std::to_string(i) + "/id").c_str(), MPV_FORMAT_INT64, &id) < 0)
                continue;

            if (id == sid) {
                char *lang = mpv_get_property_string(mpv_ctx, ("track-list/" + std::to_string(i) + "/lang").c_str());

                if (!lang)
                    return "";

                std::string result(lang);
                mpv_free(lang);
                return result;
            }
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