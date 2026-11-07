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
    private:
        mpv_handle *mpv_ctx;
        std::shared_ptr<spdlog::logger> logger;

        std::map<std::string, std::function<void(std::string[])>> keybinds{};
        std::map<std::string, std::function<void(std::string[])>> lock_keybind_callbacks{};
        std::map<std::string, std::vector<std::string>> lock_keybinds{};
    };
}
