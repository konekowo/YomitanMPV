#include "select_screen.h"

#include "screens/clay.h"

namespace YomitanMPV {
    SelectScreen::SelectScreen(
        mpv_handle *mpv_ctx,
        const std::shared_ptr<spdlog::logger> &logger,
        const std::shared_ptr<Util> &util,
        const std::shared_ptr<Display> &display,
        const std::shared_ptr<Config> &config,
        const std::shared_ptr<Network> &network
    )
        : Screen(mpv_ctx, logger, util), network(network), util(util), display(display), config(config)
    {
        util->register_lock_keybind("select_mode_lock", config->config.keybinds.select_mode, "select_mode_end", [display](std::string args[]) {
            if (args[0][0] != 'd')
                return;

            display->set_current_screen(nullptr);
        });
        util->register_lock_keybind("select_mode_lock", "ESC", "select_mode_end", [display](std::string args[]) {
            if (args[0][0] != 'd')
                return;

            display->set_current_screen(nullptr);
        });
        util->register_lock_keybind("select_mode_lock", "F12", "select_mode_devtools", [this](std::string args[]) {
            if (args[0][0] != 'd')
                return;

            Clay_SetDebugModeEnabled(true);
            if (draw)
                draw();
        });

        Screen::register_input("select_mode_lock");

        help = "Press ESC or " + config->config.keybinds.select_mode + " to exit.";
        help_text_config_ext->outline_thickness = 2;
        help_text_config_ext->outline_color.a = 100;
    }


    void SelectScreen::init() {
        util->lock_controls("select_mode_lock");

        int paused = 1;
        mpv_set_property(this->mpv_ctx, "pause", MPV_FORMAT_FLAG, &paused);
        util->set_subs_visible(false);

        token_lines = nullptr;

        const auto line = util->get_current_subtitle_line();

        if (line.empty()) {
            util->osd_show_text("There are no subtitles to select.", std::chrono::seconds(2));

            display->set_current_screen(nullptr);
            return;
        }

        tokenizing = true;
        status = "Tokenizing...";
        status_is_error = false;

        tokenize_request = network->fetch(
            Network::HttpRequest()
                .url(config->config.yomitan_api_url + "/tokenize")
                .method(Network::POST)
                .body({
                    {"text", line},
                    {"scanLength", config->config.scan_length}
                })
        );
    }

    void SelectScreen::layout() {
        const Clay_String help_clay_str = {
            .length = static_cast<int32_t>(help.length()),
            .chars = help.c_str()
        };

        const Clay_String status_clay_str = {
            .length = static_cast<int32_t>(status.length()),
            .chars = status.c_str()
        };

        const Token *hovered_token = nullptr;

        CLAY(CLAY_ID("Root"), {
            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
            .padding = CLAY_PADDING_ALL(32),
            .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP },
            .layoutDirection = CLAY_TOP_TO_BOTTOM
        }) {
            CLAY(CLAY_ID("Top"), {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
                .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_LEFT_TO_RIGHT
            }) {
                CLAY(CLAY_ID("Title"), {
                    .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_GROW() },
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                }) {
                    CLAY_TEXT(
                        CLAY_STRING("Select Mode"),
                        CLAY_TEXT_CONFIG({
                            .userData = &default_text_config_ext,
                            .textColor = { .r = 255, .g = 255, .b = 255, .a = 255 },
                            .fontSize = 48,
                        })
                    );
                    CLAY_TEXT(
                        help_clay_str,
                        CLAY_TEXT_CONFIG({
                            .userData = &help_text_config_ext,
                            .textColor = { .r = 255, .g = 255, .b = 255, .a = 100 },
                            .fontSize = 24
                        })
                    );
                }
                CLAY(CLAY_ID("Status"), {
                    .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_GROW() },
                    .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP },
                    .layoutDirection = CLAY_TOP_TO_BOTTOM
                }) {
                    if (!status.empty()) {
                        CLAY_TEXT(
                            status_clay_str,
                            CLAY_TEXT_CONFIG({
                                .userData = &default_text_config_ext,
                                .textColor = { .r = 255, .g = status_is_error ? 0.0f : 255.0f, .b = status_is_error ? 0.0f : 255.0f, .a = 255 },
                                .fontSize = 48
                            })
                        );
                    }
                }
            }
            CLAY(CLAY_ID("Bottom"), {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .padding = {
                    .bottom = 16
                },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            }) {
                if (token_lines) {
                    for (int i = 0; i < token_lines->token_lines.size(); ++i) {
                        const auto &tokens = token_lines->token_lines[i];

                        const Clay_String token_line_id = {
                            .length = static_cast<int32_t>(token_lines->line_indexes[i].length()),
                            .chars = token_lines->line_indexes[i].c_str()
                        };

                        CLAY(Clay__HashString(token_line_id, 0, 0), {
                            .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                            .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
                            .layoutDirection = CLAY_LEFT_TO_RIGHT
                        }) {
                            for (int j = 0; j < tokens.size(); ++j) {
                                const auto &token = tokens[j];

                                const Clay_String token_str = {
                                    .length = static_cast<int32_t>(token.text.length()),
                                    .chars = token.text.c_str()
                                };

                                const Clay_String token_id = {
                                    .length = static_cast<int32_t>(token_lines->token_indexes[i][j].length()),
                                    .chars = token_lines->token_indexes[i][j].c_str()
                                };

                                CLAY(Clay__HashString(token_id, 0, 0), {
                                    .sizing = { .width = CLAY_SIZING_FIT(), .height = CLAY_SIZING_FIT() },
                                }) {
                                    const bool token_hovered = Clay_Hovered() && token.selectable;

                                    if (token_hovered)
                                        hovered_token = &token;

                                    CLAY_TEXT(
                                        token_str,
                                        CLAY_TEXT_CONFIG({
                                            .userData = &default_text_config_ext,
                                            .textColor = token_hovered ?
                                                Clay_Color{ .r = 0, .g = 119, .b = 255, .a = 255 } :
                                                Clay_Color{ .r = 255, .g = 255, .b = 255, .a = 255 },
                                            .fontSize = 58
                                        })
                                    );
                                }
                            }
                        }
                    }
                }
            }
            if (hovered_token != nullptr) {
                CLAY({
                    .id = CLAY_ID("LookupWindow"),
                    .layout = {
                        .sizing = {
                            .width = CLAY_SIZING_FIT(600),
                            .height = CLAY_SIZING_FIT(400)
                        },
                        .childAlignment = { .x = CLAY_ALIGN_X_LEFT, .y = CLAY_ALIGN_Y_TOP },
                        .layoutDirection = CLAY_TOP_TO_BOTTOM
                    },
                    .backgroundColor = { .r = 10, .g = 10, .b = 10, .a = 240 },
                    .floating = {
                        .attachPoints = {
                            .element = CLAY_ATTACH_POINT_CENTER_CENTER,
                            .parent = CLAY_ATTACH_POINT_CENTER_CENTER
                        },
                        .attachTo = CLAY_ATTACH_TO_PARENT
                    }
                }) {

                }
            }
        }
    }

    void SelectScreen::poll(const mpv_event *event) {
        if (tokenize_request.valid() && tokenizing) {
            if (tokenize_request.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                tokenizing = false;

                auto response = tokenize_request.get();

                if (!response.is_ok()) {
                    status = "Failed to tokenize! Please try again.";
                    status_is_error = true;

                    if (draw)
                        draw();

                    return;
                }

                const nlohmann::json json = response.json();

                token_lines = TokenLines::from_json(json);

                status = "";

                if (draw)
                    draw();
            }
        }
    }

    void SelectScreen::destroy() {
        util->unlock_controls("select_mode_lock");

        int paused = 0;
        mpv_set_property(this->mpv_ctx, "pause", MPV_FORMAT_FLAG, &paused);
        util->set_subs_visible(true);

        tokenize_request = std::future<Network::HttpResponse>();
    }

    std::unique_ptr<SelectScreen::TokenLines> SelectScreen::TokenLines::from_json(const nlohmann::json &json) {
        auto token_lines = std::make_unique<TokenLines>();

        std::vector<Token> tokens;
        for (const auto& token : json.at(0).at("content")) {
            std::string text;

            bool has_headwords = false;

            for (const auto& part : token) {
                text += part.at("text").get<std::string>();

                if (part.contains("headwords"))
                    has_headwords = true;
            }

            if (text == "\n") {
                token_lines->token_lines.push_back(std::move(tokens));
                tokens.clear();
            } else {
                tokens.push_back({
                    .text = std::move(text),
                    .selectable = has_headwords
                });
            }
        }

        token_lines->token_lines.push_back(std::move(tokens));

        if (!tokens.empty()) {
            token_lines->token_lines.push_back(tokens);
        }

        for (int i = 0; i < token_lines->token_lines.size(); ++i) {
            token_lines->line_indexes.push_back("TokenLine" + std::to_string(i));
            std::vector<std::string> line_indexes;
            for (int j = 0; j < token_lines->token_lines[i].size(); ++j) {
                line_indexes.push_back("Token" + std::to_string(i) + "_" + std::to_string(j));
            }
            token_lines->token_indexes.push_back(line_indexes);
        }

        token_lines->json = json;

        return std::move(token_lines);
    }
}
