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

        try {
            if (std::filesystem::exists(config_path)) {
                std::ifstream file(config_path);

                if (!file.is_open()) {
                    throw std::runtime_error("Failed to open config file for reading");
                }

                file.exceptions(std::ios::badbit);

                nlohmann::json json;
                file >> json;
                file.close();

                json.get_to(*this);
            } else {
                std::ofstream file(config_path);

                if (!file.is_open()) {
                    throw std::runtime_error("Failed to create config file");
                }

                file.exceptions(std::ios::badbit);

                file << nlohmann::json(*this).dump(4);
                file.close();

                if (!file) {
                    throw std::runtime_error("Failed to write config file");
                }

                logger->info("Config file created: {}", config_path.string());
                util->osd_show_text(
                    "[" + std::string(PLUGIN_NAME) + "] Config file created: " + config_path.string(),
                    std::chrono::seconds(5)
                );
            }
        } catch (const nlohmann::json::exception& e) {
            logger->error(
                "Config JSON error in '{}': {}",
                config_path.string(),
                e.what()
            );
        } catch (const std::filesystem::filesystem_error& e) {
            logger->error(
                "Config filesystem error in '{}': {}",
                config_path.string(),
                e.what()
            );
        } catch (const std::exception& e) {
            logger->error(
                "Config file error in '{}': {}",
                config_path.string(),
                e.what()
            );
        }
    }
}
