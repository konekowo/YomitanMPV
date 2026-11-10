#include "display.h"

#define CLAY_IMPLEMENTATION
#include "clay.h"

const auto UI_LAYER_ID = std::to_string(12329);
const auto  UI_MEASURE_LAYER_ID = std::to_string(12330);

constexpr float BASE_HEIGHT = 1080.0f;

static std::string color_to_ass_hex(const Clay_Color color) {
    auto toByte = [](const float v) -> uint8_t {
        return static_cast<uint8_t>(
            std::lround(std::clamp(v, 0.0f, 255.0f))
        );
    };

    return std::format("&H{:02X}{:02X}{:02X}&", toByte(color.b), toByte(color.g), toByte(color.r));
}

static std::string alpha_to_ass_hex(const float a) {
    return std::format("&H{:02X}&", static_cast<uint8_t>(255 - std::lround(std::clamp(a, 0.0f, 255.0f))));
}

namespace YomitanMPV {
    Display::Display(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger)
        : mpv_ctx(mpv_ctx), logger(logger)
    {
        const uint32_t totalMemorySize = Clay_MinMemorySize();

        clayArenaMemory = malloc(totalMemorySize);

        clayArena = Clay_CreateArenaWithCapacityAndMemory(totalMemorySize, clayArenaMemory);

        Clay_Initialize(clayArena, { 800, 600 }, {
            .errorHandlerFunction = [](Clay_ErrorData errorText) {
                static_cast<Display*>(errorText.userData)->handle_error(errorText.errorType, errorText.errorText);
            },
            .userData = this
        });

        Clay_SetMeasureTextFunction([](Clay_StringSlice text, Clay_TextElementConfig *config, void *userData) {
            return static_cast<Display*>(userData)->measure_text(text, config);
        }, this);

        Clay_SetLayoutDimensions({ .width = static_cast<float>(width), .height = static_cast<float>(height) });

        font_id_map[0] = "Arial";

        mpv_observe_property(mpv_ctx, 0, "osd-width", MPV_FORMAT_INT64);
        mpv_observe_property(mpv_ctx, 0, "osd-height", MPV_FORMAT_INT64);

        Clay_SetPointerState({ .x = 0, .y = 0 }, false);
    }

    void Display::poll(const mpv_event *event) {
        if (event->event_id == MPV_EVENT_PROPERTY_CHANGE) {
            const auto *prop = static_cast<mpv_event_property *>(event->data);
            if (std::string(prop->name) == "osd-width" && prop->format == MPV_FORMAT_INT64 && prop->data) {
                width = *static_cast<int64_t *>(prop->data);
            } else if (std::string(prop->name) == "osd-height" && prop->format == MPV_FORMAT_INT64 && prop->data) {
                height = *static_cast<int64_t *>(prop->data);
                scale = static_cast<float>(height) / BASE_HEIGHT;
                if (current_screen)
                    current_screen->set_scale(scale);
            }
        }

        if (last_width != width || last_height != height) {
            Clay_SetLayoutDimensions({ .width = static_cast<float>(width) / scale, .height = static_cast<float>(height) / scale });
            if (current_screen) {
                current_screen->set_width(width);
                current_screen->set_height(height);
            }
            draw();
        }

        if (current_screen) {
            current_screen->poll(event);
        }

        last_width = width;
        last_height = height;
    }

    void Display::draw() {
        if (current_screen) {
            const auto osd_width = std::to_string(static_cast<int>(std::round(static_cast<float>(width) / scale)));
            const auto osd_height = std::to_string(static_cast<int>(std::round(static_cast<float>(height) / scale)));

            Clay_BeginLayout();

            current_screen->layout();

            const Clay_RenderCommandArray commands = Clay_EndLayout();

            std::string ass_events;

            for (int i = 0; i < commands.length; i++) {
                const auto *command = &commands.internalArray[i];
                const auto bounding_box = command->boundingBox;

                std::string ass_event;

                switch (command->commandType) {
                    case CLAY_RENDER_COMMAND_TYPE_NONE:
                        break;
                    case CLAY_RENDER_COMMAND_TYPE_RECTANGLE: {
                        const auto data = command->renderData.rectangle;

                        ass_event.append("{");
                        ass_event.append("\\an7");
                        ass_event.append("\\pos(").append(std::to_string(bounding_box.x)).append(",").append(std::to_string(bounding_box.y)).append(")");
                        ass_event.append("\\bord0");
                        ass_event.append("\\shad0");
                        ass_event.append("\\1c").append(color_to_ass_hex(data.backgroundColor));
                        ass_event.append("\\1a").append(alpha_to_ass_hex(data.backgroundColor.a));
                        ass_event.append("\\p1");
                        ass_event.append("}");

                        ass_event.append("m 0 0 l ").append(std::to_string(bounding_box.width)).append(" 0 l ")
                            .append(std::to_string(bounding_box.width)).append(" ").append(std::to_string(bounding_box.height))
                            .append(" l 0 ").append(std::to_string(bounding_box.height));

                        ass_event.append("{\\p0}");

                        break;
                    }
                    case CLAY_RENDER_COMMAND_TYPE_BORDER: {
                        const auto data = command->renderData.border;

                        ass_event.append("{");
                        ass_event.append("\\an7");
                        ass_event.append("\\pos(").append(std::to_string(bounding_box.x)).append(",").append(std::to_string(bounding_box.y)).append(")");
                        ass_event.append("\\bord0");
                        ass_event.append("\\shad0");
                        ass_event.append("\\1c").append(color_to_ass_hex(data.color));
                        ass_event.append("\\1a").append(alpha_to_ass_hex(data.color.a));
                        ass_event.append("\\p1");
                        ass_event.append("}");

                        auto append_rect = [&](const float x0, const float y0, const float x1, const float y1) {
                            if (x0 >= x1 || y0 >= y1)
                                return;

                            ass_event.append(" m ")
                                .append(std::to_string(x0)).append(" ")
                                .append(std::to_string(y0)).append(" l ")
                                .append(std::to_string(x1)).append(" ")
                                .append(std::to_string(y0)).append(" ")
                                .append(std::to_string(x1)).append(" ")
                                .append(std::to_string(y1)).append(" ")
                                .append(std::to_string(x0)).append(" ")
                                .append(std::to_string(y1));
                        };

                        append_rect(0, 0, bounding_box.width, data.width.top);
                        append_rect(bounding_box.width - static_cast<float>(data.width.right),
                            data.width.top, bounding_box.width, bounding_box.height - static_cast<float>(data.width.bottom));
                        append_rect(0, bounding_box.height - static_cast<float>(data.width.bottom), bounding_box.width, bounding_box.height);
                        append_rect(0, data.width.top, data.width.left, bounding_box.height - static_cast<float>(data.width.bottom));

                        ass_event.append("{\\p0}");

                        break;
                    }
                    case CLAY_RENDER_COMMAND_TYPE_TEXT: {
                        const auto data = command->renderData.text;

                        std::shared_ptr<TextConfigExt> data_ext;

                        if (command->userData) {
                            data_ext = *static_cast<std::shared_ptr<TextConfigExt>*>(command->userData);
                        } else {
                            data_ext = std::make_shared<TextConfigExt>();
                        }

                        ass_event.append("{");
                        ass_event.append("\\an7");
                        ass_event.append("\\pos(").append(std::to_string(bounding_box.x)).append(",").append(std::to_string(bounding_box.y)).append(")");
                        ass_event.append("\\fs").append(std::to_string(data.fontSize));

                        if (const auto entry = font_id_map.find(data.fontId); entry != font_id_map.end()) {
                            ass_event.append("\\fn").append(entry->second);
                        } else {
                            logger->warn("[Display] Invalid font id: {}", data.fontId);
                        }

                        ass_event.append("\\fsp").append(std::to_string(data.letterSpacing));

                        ass_event.append("\\1c").append(color_to_ass_hex(data.textColor));
                            ass_event.append("\\3c").append(color_to_ass_hex(data_ext->outline_color));
                        ass_event.append("\\1a").append(alpha_to_ass_hex(data.textColor.a));
                        ass_event.append("\\3a").append(alpha_to_ass_hex(data_ext->outline_color.a));

                        ass_event.append("\\shad0");
                        if (data_ext)
                            ass_event.append("\\bord").append(std::to_string(data_ext->outline_thickness));

                        ass_event.append("}");
                        ass_event.append(std::string(data.stringContents.chars, data.stringContents.length));
                        break;
                    }
                    case CLAY_RENDER_COMMAND_TYPE_IMAGE:
                        break;
                    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_START:
                        break;
                    case CLAY_RENDER_COMMAND_TYPE_SCISSOR_END:
                        break;
                    case CLAY_RENDER_COMMAND_TYPE_CUSTOM:
                        break;
                }

                ass_events.append(ass_event);
                if (i < commands.length - 1) {
                    ass_events.append("\n");
                }
            }

            const char *cmd[] = {
                "osd-overlay",
                UI_LAYER_ID.c_str(),
                "ass-events",
                ass_events.c_str(),
                osd_width.c_str(),
                osd_height.c_str(),
                "0", // z
                "no", // hidden
                "no", // compute_bounds
                nullptr
            };

            mpv_command(mpv_ctx, cmd);

            ui_layer_drawn = true;
        } else if (ui_layer_drawn) {
            const char *cmd[] = {
                "osd-overlay",
                UI_LAYER_ID.c_str(),
                "none",
                "",
                nullptr
            };

            mpv_command(mpv_ctx, cmd);

            ui_layer_drawn = false;
        }
    }

    void Display::set_current_screen(const std::shared_ptr<Screen>& screen) {
        if (current_screen) {
            current_screen->set_draw_cb(nullptr);
            current_screen->destroy();
        }

        current_screen = screen;

        // screen supplied can be nullptr to clear display
        if (current_screen) {
            current_screen->set_draw_cb([this] { draw(); });
            current_screen->init();
        }

        draw();
    }

    std::shared_ptr<Screen> Display::get_current_screen() {
        return current_screen;
    }

    Clay_Dimensions Display::measure_text(const Clay_StringSlice &text, Clay_TextElementConfig *config) const {
        const auto str = std::string(text.chars, text.length);

        std::string ass_event_styling = "{";
        ass_event_styling.append("\\an7");
        ass_event_styling.append("\\pos(0,0)");
        ass_event_styling.append("\\fs").append(std::to_string(config->fontSize));

        if (const auto entry = font_id_map.find(config->fontId); entry != font_id_map.end()) {
            ass_event_styling.append("\\fn").append(entry->second);
        } else {
            logger->warn("[Display] Invalid font id: {}", config->fontId);
        }

        ass_event_styling.append("\\fsp").append(std::to_string(config->letterSpacing));

        ass_event_styling.append("\\bord0");
        ass_event_styling.append("\\shad0");

        switch(config->wrapMode) {
            case CLAY_TEXT_WRAP_WORDS:
                ass_event_styling.append("\\q0");
                break;
            case CLAY_TEXT_WRAP_NEWLINES:
                ass_event_styling.append("\\q1");
                break;
            case CLAY_TEXT_WRAP_NONE:
                ass_event_styling.append("\\q2");
                break;
        }

        ass_event_styling.append("}");

        // measure with markers hack from https://github.com/BrenoAqua/Yomipv/blob/main/scripts/yomipv/interface/selector/renderer.lua#L34

        const std::string ass_event_text_with_markers = ass_event_styling + "|" + str + "|";
        const std::string ass_event_markers = ass_event_styling + "||";

        const auto bounds = get_bounds(ass_event_styling + str);
        const auto bounds_text_with_markers = get_bounds(ass_event_text_with_markers);
        const auto bounds_markers = get_bounds(ass_event_markers);

        return {
            .width = static_cast<float>(std::max(0.0, bounds_text_with_markers.width() - bounds_markers.width())),
            .height = static_cast<float>(bounds.height())
        };
    }

    Display::MPVBounds Display::get_bounds(const std::string &ass_event) const {
        const auto osd_width = std::to_string(static_cast<int>(std::round(static_cast<float>(width) / scale)));
        const auto osd_height = std::to_string(static_cast<int>(std::round(static_cast<float>(height) / scale)));

        const char *cmd[] = {
            "osd-overlay",
            UI_MEASURE_LAYER_ID.c_str(),
            "ass-events",
            ass_event.c_str(),
            osd_width.c_str(),
            osd_height.c_str(),
            "0", // z
            "yes", // hidden
            "yes", // compute_bounds
            nullptr
        };

        mpv_node result{};
        MPVBounds bounds{};

        if (mpv_command_ret(mpv_ctx, cmd, &result) != MPV_ERROR_SUCCESS) {
            return {
                .x0 = 0,
                .y0 = 0,
                .x1 = 0,
                .y1 = 0
            };
        }

        if (result.format == MPV_FORMAT_NODE_MAP) {
            for (int i = 0; i < result.u.list->num; ++i) {
                double* target = nullptr;

                if (!strcmp(result.u.list->keys[i], "x0")) target = &bounds.x0;
                else if (!strcmp(result.u.list->keys[i], "y0")) target = &bounds.y0;
                else if (!strcmp(result.u.list->keys[i], "x1")) target = &bounds.x1;
                else if (!strcmp(result.u.list->keys[i], "y1")) target = &bounds.y1;

                if (!target)
                    continue;

                if (result.u.list->values[i].format == MPV_FORMAT_DOUBLE) {
                    *target = result.u.list->values[i].u.double_;
                } else if (result.u.list->values[i].format == MPV_FORMAT_INT64) {
                    *target = static_cast<double>(result.u.list->values[i].u.int64);
                }
            }
        }

        mpv_free_node_contents(&result);

        const char *cmd_clear[] = {
            "osd-overlay",
            UI_MEASURE_LAYER_ID.c_str(),
            "none",
            "",
            nullptr
        };

        mpv_command(mpv_ctx, cmd_clear);

        return bounds;
    }

    void Display::handle_error(const Clay_ErrorType &errorType, const Clay_String &errorText) const {
        logger->error("[Display] (Clay {}): {}", std::to_string(errorType), std::string(errorText.chars, errorText.length));
    }

    Display::~Display() {
        free(clayArenaMemory);
    }
}
