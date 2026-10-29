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

        while(true) {
            const mpv_event *event = mpv_wait_event(mpv_ctx, -1);

            if (event->event_id == MPV_EVENT_NONE) {
                continue;
            }

            if (event->event_id == MPV_EVENT_SHUTDOWN) {
                break;
            }

            if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
                const auto *prop = static_cast<mpv_event_property *>(event->data);

                if (event->reply_userdata == 1) {
                    if (prop->format == MPV_FORMAT_INT64 && prop->data) {
                        const int64_t sid = *static_cast<int64_t *>(prop->data);

                        logger->info("Subtitles set to {}", util->get_subtitle_language(sid));
                        

                    }
                }
            }
        }
    }
}