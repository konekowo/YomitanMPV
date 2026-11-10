#include "screen.h"

namespace YomitanMPV {
    Screen::Screen(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<Util> &util)
        : mpv_ctx(mpv_ctx), logger(logger), util(util)
    {
        default_text_config_ext->outline_thickness = 2;
    }

    void Screen::register_input(const std::string &lock_name) {
        std::function<void(const std::string&, const std::string&, const std::function<void(std::string[])>&)> register_keybind;

        if (lock_name.empty()) {
            register_keybind = [this](const std::string &keybind, const std::string &name, const std::function<void(std::string[])>& callback) {
                util->register_keybind(keybind, name, callback);
            };
        } else {
            register_keybind = [this, lock_name](const std::string &keybind, const std::string &name, const std::function<void(std::string[])>& callback) {
                util->register_lock_keybind(lock_name, keybind, name, callback);
            };
        }

        std::function mouse_input_function = [this] {
            int64_t x = 0, y = 0;
            int hovered = 0;

            mpv_node node;

            if (mpv_get_property(mpv_ctx,"mouse-pos", MPV_FORMAT_NODE, &node) != MPV_ERROR_SUCCESS)
                return;

            const mpv_node_list *map = node.u.list;

            for (int i = 0; i < map->num; i++) {
                if (std::string(map->keys[i]) == "x") {
                    if (map->values[i].format == MPV_FORMAT_INT64) {
                        x = map->values[i].u.int64;
                    }
                } else if (std::string(map->keys[i]) == "y") {
                    if (map->values[i].format == MPV_FORMAT_INT64) {
                        y = map->values[i].u.int64;
                    }
                } else if (std::string(map->keys[i]) == "hover") {
                    if (map->values[i].format == MPV_FORMAT_FLAG) {
                        hovered = map->values[i].u.flag;
                    }
                }
            }

            const float clay_width = static_cast<float>(width) / scale;
            const float clay_height = static_cast<float>(height) / scale;

            Clay_SetPointerState(hovered ? Clay_Vector2{
                .x = static_cast<float>(x) * (clay_width / static_cast<float>(width)),
                .y = static_cast<float>(y) * (clay_height / static_cast<float>(height))
            } : Clay_Vector2{ .x = -9999, .y = -9999 }, mouse_down);

            if (draw)
                draw();
        };

        register_keybind("MOUSE_MOVE", "input_mouse_move", [mouse_input_function](std::string args[]) {
            mouse_input_function();
        });

        register_keybind("MBTN_LEFT", "input_mouse_left", [this, mouse_input_function](std::string args[]) {
            mouse_down = true;

            mouse_input_function();

            mouse_down = false;
        });
    }

    void Screen::set_width(int64_t width) {
        this->width = width;
    }

    void Screen::set_height(int64_t height) {
        this->height = height;
    }

    void Screen::set_scale(float scale) {
        this->scale = scale;
    }

    void Screen::set_draw_cb(const std::function<void()> &draw_cb) {
        draw = draw_cb;
    }
}
