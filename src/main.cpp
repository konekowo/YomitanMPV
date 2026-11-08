#include "main.h"

#include "spdlog/spdlog.h"
#include "build_info.h"

extern "C" {
int mpv_open_cplugin(mpv_handle *handle) {
    YomitanMPV::network_pre_init();

    YomitanMPV::Context{handle};

    YomitanMPV::network_destroy();
    return 0;
}
}

namespace YomitanMPV {
    Context::Context(mpv_handle *mpv_ctx) {
        this->logger = std::make_shared<spdlog::logger>(PLUGIN_NAME, LOGGER_SINK);
        spdlog::set_level(spdlog::level::debug);
        logger->set_level(spdlog::get_level());

        this->mpv_ctx = mpv_ctx;
        this->util = std::make_shared<Util>(mpv_ctx, logger);
        this->config = std::make_shared<Config>(mpv_ctx, logger, util);
        this->network = std::make_shared<Network>(logger);

        logger->info("{} ({}) Loaded!", PLUGIN_NAME, PLUGIN_VERSION);

        mpv_observe_property(mpv_ctx, 1, "sid", MPV_FORMAT_INT64);

        util->register_keybind(config->config.keybinds.select_mode, "select_mode", [this](std::string[]) {
            enter_select_mode();
        });

        util->register_lock_keybind("select_mode_lock", config->config.keybinds.select_mode, "select_mode_end", [this](std::string[]) {
            exit_select_mode();
        });
        util->register_lock_keybind("select_mode_lock", "ESC", "select_mode_end", [this](std::string[]) {
            exit_select_mode();
        });

        while(true) {
            const mpv_event *event = mpv_wait_event(mpv_ctx, -1);

            if (event->event_id == MPV_EVENT_NONE) {
                continue;
            }

            if (event->event_id == MPV_EVENT_SHUTDOWN) {
                break;
            }

            if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
                auto *msg = static_cast<mpv_event_client_message *>(event->data);

                if (msg->num_args > 0) {
                    if (std::string(msg->args[0]).compare(0, std::string("keybind_").length(), "keybind_") == 0) {
                        util->keybind_event_poll(msg);
                    }
                    if (std::string(msg->args[0]).compare(0, std::string("lock_keybind_").length(), "lock_keybind_") == 0) {
                        util->lock_keybind_event_poll(msg);
                    }
                }
            }

            if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                const auto *prop = static_cast<mpv_event_property *>(event->data);

                if (event->reply_userdata == 1) {
                    if (std::string(prop->name) == "sid" && prop->format == MPV_FORMAT_INT64 && prop->data) {
                        const int64_t sid = *static_cast<int64_t *>(prop->data);

                        std::string lang = util->get_subtitle_language(sid);

                        logger->info("Subtitles set to {}", lang);
                        is_enabled = lang == "ja";

                    }
                }
            }
        }
    }

    template <typename T, typename F>
    static void on_done(std::future<T> future, F callback) {
        std::thread([f = std::move(future), cb = std::move(callback)]() mutable {
            T result = f.get();
            cb(result);
        }).detach();
    }

    void Context::enter_select_mode() {
        util->lock_controls("select_mode_lock");

        int paused = 1;
        mpv_set_property(this->mpv_ctx, "pause", MPV_FORMAT_FLAG, &paused);
        util->set_subs_visible(false);

        select_mode = true;

        std::string line = util->get_current_subtitle_line();

        if (line.empty()) {
            util->osd_show_text("There are no subtitles to select.", std::chrono::seconds(5));

            exit_select_mode();
            return;
        }

        util->add_text(0, "Tokenizing...", 0, 0);

        on_done(network->fetch(
            Network::HttpRequest()
                .url(config->config.yomitan_api_url + "/tokenize")
                .method(Network::POST)
                .body({
                    {"text", line},
                    {"scanLength", line.length()}
                })
        ), [this, line](const Network::HttpResponse& response) {
            if (!select_mode)
                return;

            if (util->get_current_subtitle_line() != line)
                return;

            util->remove_overlay(0);
        });


    }

    void Context::exit_select_mode() {
        select_mode = false;

        util->remove_overlay(0);

        util->unlock_controls("select_mode_lock");

        int paused = 0;
        mpv_set_property(this->mpv_ctx, "pause", MPV_FORMAT_FLAG, &paused);
        util->set_subs_visible(true);
    }
}
