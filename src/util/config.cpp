#include "config.h"

#include <build_info.h>
#include <fstream>

#define YOMITANMPV_CONFIG_FILE_NAME "YomitanMPV-config.json"

namespace YomitanMPV {
    Config::Config(mpv_handle *mpv_ctx, const std::shared_ptr<spdlog::logger> &logger, const std::shared_ptr<Util> &util) {
        this->mpv_ctx = mpv_ctx;
        this->logger = logger;
        this->util = util;

        const std::filesystem::path config_dir = std::filesystem::path(util->get_mpv_config_path()) / "script-opts";

        std::filesystem::create_directories(config_dir);

        std::filesystem::path config_path = config_dir / YOMITANMPV_CONFIG_FILE_NAME;

        if (std::filesystem::exists(config_path)) {
            std::ifstream file(config_path);

            nlohmann::json json;
            file >> json;

            file.close();

            json.get_to(*this);

            logger->info("Config file loaded: {}", nlohmann::json(*this).dump());
        } else {
            std::ofstream file(config_path);
            file << nlohmann::json(*this).dump(4);

            file.close();

            logger->info("Config file created: {}", config_path.string());
            util->osd_show_text("[" + std::string(PLUGIN_NAME) + "] Config file created: " + config_path.string(), std::chrono::seconds(5));
        }
    }
}
