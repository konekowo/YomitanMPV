#include "util.h"

const std::string OVERLAY_VIRTUAL_WIDTH = std::to_string(1920);
const std::string OVERLAY_VIRTUAL_HEIGHT = std::to_string(1080);

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

    void Util::register_lock_keybind(const std::string& lock_name, const std::string &keybind, const std::string &name, const std::function<void(std::string[])>& callback) {
        const char *client_name = mpv_client_name(mpv_ctx);

        std::string command;

        if (lock_keybinds.find(name) == lock_keybinds.end()) {
            command = keybind + " script-message-to " + std::string(client_name) + " lock_keybind_" + name;
            lock_keybinds[name] = std::vector<std::string>();
        } else {
            for (const auto& lock_keybind : lock_keybinds[name]) {
                command += lock_keybind + " script-message-to " + std::string(client_name) + " lock_keybind_" + name + "\n";
            }

            command += keybind + " script-message-to " + std::string(client_name) + " lock_keybind_" + name;
        }

        const char *define[] = {
            "define-section",
            lock_name.c_str(),
            command.c_str(),
            "force",
            nullptr
        };

        mpv_command(mpv_ctx, define);

        lock_keybind_callbacks[name] = callback;
        lock_keybinds[name].push_back(keybind);
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

    void Util::lock_keybind_event_poll(const mpv_event_client_message *message) {
        const std::string keybind_name = std::string(message->args[0]).substr(std::string("lock_keybind_").length());

        std::string args[message->num_args - 1];

        for (int i = 1; i < message->num_args; i++) {
            args[i - 1] = std::string(message->args[i]);
        }

        if (lock_keybind_callbacks.find(keybind_name) != lock_keybind_callbacks.end()) {
            lock_keybind_callbacks[keybind_name](args);
        }
    }

    void Util::lock_controls(const std::string &lock_name) const {
        const char *enable[] = {
            "enable-section",
            lock_name.c_str(),
            "exclusive",
            nullptr
        };

        mpv_command(mpv_ctx, enable);
    }

    void Util::unlock_controls(const std::string &lock_name) const {
        const char *disable[] = {
            "disable-section",
            lock_name.c_str(),
            nullptr
        };

        mpv_command(mpv_ctx, disable);
    }

    void Util::set_subs_visible(const bool value) const {
        const char *cmd[] = {
            "set",
            "sub-visibility",
            value ? "yes" : "no",
            nullptr
        };

        mpv_command(mpv_ctx, cmd);
    }

    std::string Util::get_current_subtitle_line() const {
        char *mpv_sub_text;

        if (mpv_get_property(this->mpv_ctx, "sub-text", MPV_FORMAT_STRING, &mpv_sub_text) < 0)
            throw std::runtime_error("Failed to get subtitle line!");

        auto sub_text = std::string(mpv_sub_text);

        mpv_free(mpv_sub_text);

        return sub_text;
    }

    int Util::add_overlay_event(const std::string& overlay_event, Bounds *bounds, const bool hidden) {
        const int id = current_event_id++;

        const std::string id_str = std::to_string(id);

        const char *cmd[] = {
            "osd-overlay",
            id_str.c_str(),
            "ass-events",
            overlay_event.c_str(),
            OVERLAY_VIRTUAL_WIDTH.c_str(),
            OVERLAY_VIRTUAL_HEIGHT.c_str(),
            "0",
            hidden ? "yes" : "no",
            bounds ? "yes" : "no",
            nullptr
        };

        if (bounds) {
            mpv_node result{};

            mpv_command_ret(mpv_ctx, cmd, &result);

            if (result.format == MPV_FORMAT_NODE_MAP) {
                for (int i = 0; i < result.u.list->num; ++i) {
                    double* target = nullptr;

                    if (!strcmp(result.u.list->keys[i], "x0")) target = &bounds->x0;
                    else if (!strcmp(result.u.list->keys[i], "y0")) target = &bounds->y0;
                    else if (!strcmp(result.u.list->keys[i], "x1")) target = &bounds->x1;
                    else if (!strcmp(result.u.list->keys[i], "y1")) target = &bounds->y1;

                    if (!target)
                        continue;

                    if (result.u.list->values[i].format == MPV_FORMAT_DOUBLE) {
                        *target = result.u.list->values[i].u.double_;
                    } else if (result.u.list->values[i].format == MPV_FORMAT_INT64) {
                        *target = static_cast<double>(result.u.list->values[i].u.int64);
                    }
                }
            }

            mpv_free_node_contents(&result);
        } else {
            mpv_command(mpv_ctx, cmd);
        }

        return id;
    }

    void Util::remove_overlay(const int id) const {
        const std::string id_str = std::to_string(id);

        const char *cmd[] = {
            "osd-overlay",
            id_str.c_str(),
            "none",
            "",
            nullptr
        };

        mpv_command(mpv_ctx, cmd);
    }

    std::vector<int> Util::add_broken_up_text(const std::vector<std::string>& text) {
        std::vector<int> ids;

        ids.reserve(text.size());

        int start_y = 0;

        std::vector<std::vector<std::string>> lines;

        {
            std::vector<std::string> line;
            for (const auto & segment : text) {
                line.push_back(segment);

                if (segment == "\n") {
                    start_y += 48;

                    lines.push_back(line);
                    line.clear();
                }
            }

            if (!line.empty())
                lines.push_back(line);
        }

        std::vector<std::vector<Bounds>> lines_widths;
        for (const auto & line : lines) {
            std::vector<Bounds> widths;

            for (const auto& segment : line) {
                Bounds bounds{};

                int id = add_overlay_event(
                    OverlayBuilder()
                        .type(TEXT)
                        .text(segment)
                        .anchor(TOP_LEFT)
                        .x(0)
                        .y(0)
                        .build(),
                    &bounds,
                    true
                );

                remove_overlay(id);

                widths.push_back(bounds);
            }

            lines_widths.push_back(widths);
        }

        std::vector<double> widths;

        for (const auto & line : lines_widths) {
            double width = 0;

            for (auto segment : line) {
                width += segment.width();
            }

            widths.push_back(width);
        }

        for (int i = 0; i < lines.size(); i++) {
            double width = widths[i];
            const std::vector<Bounds>& line_bounds = lines_widths[i];
            const std::vector<std::string>& line = lines[i];

            double x_pos = 0;

            for (int j = 0; j < line.size(); ++j) {
                std::string segment = line[j];
                Bounds bounds = line_bounds[j];

                ids.push_back(
                    add_overlay_event(
                        OverlayBuilder()
                            .type(TEXT)
                            .text(segment)
                            .anchor(BOTTOM_CENTER)
                            .x(static_cast<int>(1920 / 2.0f - width / 2.0f + x_pos))
                            .y(1080 - (start_y + 15))
                            .build()
                    )
                );

                x_pos += bounds.width();
            }

            start_y -= 48;
        }
        return ids;
    }

    std::string OverlayBuilder::build() const {
        std::string result = "{";

        result.append("\\an").append(std::to_string(_anchor));
        result.append("\\pos(").append(std::to_string(_x)).append(",").append(std::to_string(_y)).append(")");
        result.append("\\1c&H").append(_fill_color).append("&");
        result.append("\\3c&H").append(_outline_color).append("&");

        if (_type == TEXT)
            result.append("\\fs").append(std::to_string(_font_size));

        result.append("}");

        if (_type == TEXT)
            result.append(_text);

        return result;
    }
}
