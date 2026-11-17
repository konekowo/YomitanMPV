#pragma once

#include <atomic>
#include <future>
#include <memory>
#include <queue>
#include <vector>
#include <spdlog/logger.h>
#include <curl/curl.h>

#include <nlohmann/json.hpp>

namespace YomitanMPV {
    void network_pre_init();
    void network_destroy();

    class Network {
    public:
        struct HttpResponse {
            CURLcode curl_status = CURLE_OK;
            std::string curl_status_message;
            long status_code = 0;

            std::string body;

            nlohmann::json json() {
                return nlohmann::json::parse(body);
            }

            [[nodiscard]] bool is_ok() const {
                return curl_status == CURLE_OK &&
                       status_code >= 200 &&
                       status_code < 300;
            }
        };

        enum HttpMethod {
            GET,
            POST
        };

        class HttpRequest {
        protected:
            std::string _url;
            HttpMethod _method{GET};
            std::vector<std::string> _headers;
            std::string _body;
        public:
            [[nodiscard]] std::string url() const;
            HttpRequest &url(const std::string &url);
            [[nodiscard]] HttpMethod method() const;
            HttpRequest &method(HttpMethod method);
            [[nodiscard]] const std::vector<std::string> &headers() const;
            HttpRequest &header(const std::string& key, const std::string &value);
            [[nodiscard]] std::string body() const;
            HttpRequest &body(const std::string& text);
            HttpRequest &body(const nlohmann::json& json);
        };

        explicit Network(const std::shared_ptr<spdlog::logger>& logger);
        ~Network();

        std::future<HttpResponse> fetch(const HttpRequest &http_request);

        bool is_active() const;
    private:
        struct Request {
            std::string url;
            HttpMethod method;
            curl_slist *headers = nullptr;
            std::string body;

            HttpResponse response;
            std::promise<HttpResponse> promise;

            ~Request() {
                curl_slist_free_all(headers);
            }
        };

        void run();
        void addRequest(std::unique_ptr<Request> request);
        void processCompleted();

        static size_t writeCallback(
            const char* data,
            size_t size,
            size_t nmemb,
            void* userdata
        );

        std::shared_ptr<spdlog::logger> logger;

        CURLM *curl_multi;
        std::thread worker;
        std::mutex mutex;
        std::queue<std::unique_ptr<Request>> pending;
        std::unordered_map<CURL*, std::unique_ptr<Request>> active;
        std::atomic<size_t> request_count{0};
        bool stopping{false};
    };
}


