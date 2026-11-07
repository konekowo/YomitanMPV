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

    void Util::register_keybind(const std::string &keybind, const std::string &name, const std::function<void(std::string[])>& callback) {
        const char *client_name = mpv_client_name(mpv_ctx);

        const std::string command = "script-message-to " + std::string(client_name) + " keybind_" + name;

        const char *args[] = {
            "keybind",
            keybind.c_str(),
            command.c_str(),
            nullptr
        };

        mpv_command(mpv_ctx, args);

        keybinds[name] = callback;
    }

    void Util::keybind_event_poll(const mpv_event_client_message *message) {
        const std::string keybind_name = std::string(message->args[0]).substr(std::string("keybind_").length());

        std::string args[message->num_args - 1];

        for (int i = 1; i < message->num_args; i++) {
            args[i - 1] = std::string(message->args[i]);
        }

        if (keybinds.find(keybind_name) != keybinds.end()) {
            keybinds[keybind_name](args);
        }

    }
}
