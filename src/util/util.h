#pragma once

#include <string>
#include <chrono>
#include <map>
#include <memory>
#include <mpv/client.h>
#include <spdlog/logger.h>

namespace YomitanMPV {
    class Util {
    public:
        explicit Util(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger>& logger);
        [[nodiscard]] std::string get_mpv_config_path() const;
        [[nodiscard]] std::string get_subtitle_language(int64_t sid) const;
        void osd_show_text(const std::string& text, std::chrono::milliseconds duration) const;
        void register_keybind(const std::string &keybind, const std::string &name, const std::function<void(std::string[])> &callback);
        void register_lock_keybind(const std::string &lock_name, const std::string &keybind, const std::string &name, const std::function<void(std::string[])>& callback);
        void keybind_event_poll(const mpv_event_client_message *message);
        void lock_keybind_event_poll(const mpv_event_client_message *message);
        void lock_controls(const std::string& lock_name) const;
        void unlock_controls(const std::string& lock_name) const;
        void set_subs_visible(bool value) const;
        [[nodiscard]] std::string get_current_subtitle_line() const;

        struct Bounds {
            double x0;
            double y0;
            double x1;
            double y1;

            double width() {
                return x1 - x0;
            }
            double height() {
                return y1 - y0;
            }
        };

        int add_overlay_event(const std::string &overlay_event, Bounds *bounds = nullptr, bool hidden = false);
        void remove_overlay(int id) const;

        std::vector<int> add_broken_up_text(const std::vector<std::string> &text);
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;

        std::map<std::string, std::function<void(std::string[])>> keybinds{};
        std::map<std::string, std::function<void(std::string[])>> lock_keybind_callbacks{};
        std::map<std::string, std::vector<std::string>> lock_keybinds{};

        int current_event_id = 0;
    };

    enum OverlayType {
        TEXT
    };

    enum OverlayAnchor {
        TOP_LEFT = 7,
        TOP_CENTER = 8,
        TOP_RIGHT = 9,
        MIDDLE_LEFT = 4,
        MIDDLE_CENTER = 5,
        MIDDLE_RIGHT = 6,
        BOTTOM_LEFT = 1,
        BOTTOM_CENTER = 2,
        BOTTOM_RIGHT = 3,
    };

    class OverlayBuilder {
        OverlayType _type = TEXT;
        int _x = 0;
        int _y = 0;
        OverlayAnchor _anchor = TOP_LEFT;
        std::string _fill_color = "FFFFFF";
        std::string _outline_color = "000000";
        int _font_size = 48;
        std::string _text;

    public:
        OverlayBuilder &type(const OverlayType type) {
            _type = type;
            return *this;
        }
        OverlayBuilder &x(const int x) {
            _x = x;
            return *this;
        }
        OverlayBuilder &y(const int y) {
            _y = y;
            return *this;
        }
        OverlayBuilder &anchor(const OverlayAnchor anchor) {
            _anchor = anchor;
            return *this;
        }
        OverlayBuilder &fill_color(const std::string &fill_color) {
            _fill_color = fill_color;
            return *this;
        }
        OverlayBuilder &outline_color(const std::string &outline_color) {
            _outline_color = outline_color;
            return *this;
        }
        OverlayBuilder &font_size(const int font_size) {
            _font_size = font_size;
            return *this;
        }
        OverlayBuilder &text(const std::string &text) {
            _text = text;
            return *this;
        }

        std::string build() const;
    };
}
