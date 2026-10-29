#pragma once

#include <future>
#include <memory>
#include <spdlog/logger.h>
#include <curl/curl.h>

namespace YomitanMPV {
    void network_pre_init();
    void network_destroy();

    struct HttpResponse {
        CURLcode curl_status = CURLE_OK;
        long status_code = 0;

        std::string body;

        [[nodiscard]] bool is_ok() const {
            return curl_status == CURLE_OK &&
                   status_code >= 200 &&
                   status_code < 300;
        }
    };

    class Network {
    public:
        explicit Network(const std::shared_ptr<spdlog::logger>& logger);
        ~Network();

        std::future<HttpResponse> fetch(const std::string& url);
    private:
        std::shared_ptr<spdlog::logger> logger;
        CURLM *curl_multi;
    };
}


