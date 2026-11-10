#include "main.h"

#include "spdlog/spdlog.h"
#include "build_info.h"
#include "screens/impl/select_screen.h"

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
        this->display = std::make_shared<Display>(mpv_ctx, logger);

        logger->info("{} ({}) Loaded!", PLUGIN_NAME, PLUGIN_VERSION);

        mpv_observe_property(mpv_ctx, 0, "sid", MPV_FORMAT_INT64);

        auto select_screen = std::make_shared<SelectScreen>(
            mpv_ctx,
            logger,
            util,
            display,
            config,
            network
        );

        util->register_keybind(config->config.keybinds.select_mode, "select_mode", [this, select_screen](std::string args[]) {
            if (args[0][0] != 'd')
                return;

            display->set_current_screen(select_screen);
        });

        while(true) {
            const mpv_event *event = mpv_wait_event(mpv_ctx, network->is_active() ? 0.1 : -1);

            display->poll(event);

            if (event->event_id == MPV_EVENT_NONE) {
                continue;
            }

            if (event->event_id == MPV_EVENT_SHUTDOWN) {
                break;
            }

            if (event->event_id == MPV_EVENT_CLIENT_MESSAGE) {
                auto *msg = static_cast<mpv_event_client_message *>(event->data);


                if (msg->num_args >= 7) {
                    if (std::string(msg->args[0]) == "key-binding") {
                        if (std::string(msg->args[1]).compare(0, std::string("keybind_").length(), "keybind_") == 0) {
                            util->keybind_event_poll(msg);
                        }
                        if (std::string(msg->args[1]).compare(0, std::string("lock_keybind_").length(), "lock_keybind_") == 0) {
                            util->lock_keybind_event_poll(msg);
                        }
                    }
                }
            }

            if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                const auto *prop = static_cast<mpv_event_property *>(event->data);

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
