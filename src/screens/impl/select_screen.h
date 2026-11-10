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
            const std::shared_ptr<Util> &util,
            const std::shared_ptr<Display> &display,
            const std::shared_ptr<Config> &config,
            const std::shared_ptr<Network> &network
        );

        void init() override;
        void layout() override;
        void poll(const mpv_event *event) override;
        void destroy() override;

    private:
        struct Token {
            std::string text;
            bool selectable = false;
        };

        struct TokenLines {
            std::vector<std::vector<Token>> token_lines;
            std::vector<std::string> line_indexes;
            std::vector<std::vector<std::string>> token_indexes;
            nlohmann::json json;

            static std::unique_ptr<TokenLines> from_json(const nlohmann::json& json);
        };

        std::shared_ptr<Network> network;
        std::shared_ptr<Util> util;
        std::shared_ptr<Display> display;
        std::shared_ptr<Config> config;

        std::future<Network::HttpResponse> tokenize_request;
        bool tokenizing = false;
        std::unique_ptr<TokenLines> token_lines = nullptr;

        std::string help;
        std::shared_ptr<TextConfigExt> help_text_config_ext = std::make_shared<TextConfigExt>();
        std::string status;
        bool status_is_error = false;


    };
}
