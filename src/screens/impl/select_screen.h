#pragma once

#include "network/network.h"
#include "screens/display.h"
#include "screens/screen.h"
#include "util/config.h"
#include "util/util.h"

namespace YomitanMPV {
    class SelectScreen : public Screen {
    public:
        SelectScreen(
            mpv_handle *mpv_ctx,
            const std::shared_ptr<spdlog::logger> &logger,
            const std::shared_ptr<Display> &display,
            const std::shared_ptr<Config> &config,
            const std::shared_ptr<Util> &util,
            const std::shared_ptr<Network> &network
        );

        void init() override;
        void layout() override;
        void poll(const mpv_event *event) override;
        void destroy() override;

    private:
        std::shared_ptr<Network> network;
        std::shared_ptr<Util> util;
        std::shared_ptr<Display> display;
        std::shared_ptr<Config> config;

        std::future<Network::HttpResponse> tokenize_request;
        bool tokenizing = false;

        std::string help;
        std::string status = "Tokenizing...";
        bool status_is_error = false;

        std::vector<std::vector<std::string>> segment_lines;
        std::vector<std::string> segment_line_indexes;
    };
}
