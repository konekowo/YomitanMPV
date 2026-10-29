#include "network.h"

namespace YomitanMPV {
    void network_pre_init() {
        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK) {
            throw std::runtime_error("curl_global_init() failed");
        }
    }

    void network_destroy() {
        curl_global_cleanup();
    }

    Network::Network(const std::shared_ptr<spdlog::logger> &logger) {
        this->logger = logger;

        this->curl_multi = curl_multi_init();
    }

    std::future<HttpResponse> Network::fetch(const std::string& url) {

    }


    Network::~Network() {
        curl_multi_cleanup(this->curl_multi);
    }
}
