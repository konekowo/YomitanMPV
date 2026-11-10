#include "select_screen.h"


#include "screens/clay.h"

namespace YomitanMPV {
    SelectScreen::SelectScreen(
        mpv_handle *mpv_ctx,
        const std::shared_ptr<spdlog::logger> &logger,
        const std::shared_ptr<Display> &display,
        const std::shared_ptr<Config> &config,
        const std::shared_ptr<Util> &util,
        const std::shared_ptr<Network> &network
    )
        : Screen(mpv_ctx, logger), network(network), util(util), display(display), config(config)
    {
        util->register_lock_keybind("select_mode_lock", config->config.keybinds.select_mode, "select_mode_end", [display](std::string[]) {
            display->set_current_screen(nullptr);
        });
        util->register_lock_keybind("select_mode_lock", "ESC", "select_mode_end", [display](std::string[]) {
            display->set_current_screen(nullptr);
        });

        help = "Press ESC or " + config->config.keybinds.select_mode + " to exit.";
    }

    void SelectScreen::init() {
        util->lock_controls("select_mode_lock");

        int paused = 1;
        mpv_set_property(this->mpv_ctx, "pause", MPV_FORMAT_FLAG, &paused);
        util->set_subs_visible(false);

        segment_lines.clear();
        segment_line_indexes.clear();

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
                    {"scanLength", line.length()}
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
                            .textColor = { .r = 255, .g = 255, .b = 255, .a = 255 },
                            .fontSize = 48,
                        })
                    );
                    CLAY_TEXT(
                        help_clay_str,
                        CLAY_TEXT_CONFIG({
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
                                .textColor = { .r = 255, .g = status_is_error ? 0.0f : 255.0f, .b = status_is_error ? 0.0f : 255.0f, .a = 255 },
                                .fontSize = 48
                            })
                        );
                    }
                }
            }
            CLAY(CLAY_ID("Bottom"), {
                .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
                .layoutDirection = CLAY_TOP_TO_BOTTOM
            }) {
                for (int i = 0; i < segment_lines.size(); ++i) {
                    const auto segments = segment_lines.at(i);
                    segment_line_indexes[i] = "SegmentLine" + std::to_string(i);

                    const Clay_String segment_line_id = {
                        .length = static_cast<int32_t>(segment_line_indexes[i].length()),
                        .chars = segment_line_indexes[i].c_str()
                    };

                    CLAY(Clay__HashString(segment_line_id, 0, 0), {
                        .sizing = { .width = CLAY_SIZING_GROW(), .height = CLAY_SIZING_FIT() },
                        .childAlignment = { .x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_TOP },
                        .layoutDirection = CLAY_LEFT_TO_RIGHT
                    }) {
                        for (const auto& segment : segments) {
                            const Clay_String segment_str = {
                                .length = static_cast<int32_t>(segment.length()),
                                .chars = segment.c_str()
                            };

                            CLAY_TEXT(
                                segment_str,
                                CLAY_TEXT_CONFIG({
                                    .textColor = { .r = 255, .g = status_is_error ? 0.0f : 255.0f, .b = status_is_error ? 0.0f : 255.0f, .a = 255 },
                                    .fontSize = 48
                                })
                            );
                        }
                    }
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

                    draw();

                    return;
                }

                nlohmann::json json = response.json();

                std::vector<std::string> segments;
                for (auto segment : json.at(0)["content"]) {
                    if (segment == "\n") {
                        segment_lines.push_back(segment);
                    } else {
                        segments.push_back(segment.at(0)["text"]);
                    }
                }
                if (!segments.empty()) {
                    segment_lines.push_back(segments);
                }

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
}
