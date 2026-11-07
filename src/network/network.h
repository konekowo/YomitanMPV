#pragma once

#include <future>
#include <memory>
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
            curl_slist *_headers = nullptr;
            std::string _body;
        public:
            [[nodiscard]] std::string url() const;
            HttpRequest &url(const std::string &url);
            [[nodiscard]] HttpMethod method() const;
            HttpRequest &method(HttpMethod method);
            [[nodiscard]] curl_slist *headers() const;
            HttpRequest &header(const std::string& key, const std::string &value);
            [[nodiscard]] std::string body() const;
            HttpRequest &body(const std::string& text);
            HttpRequest &body(const nlohmann::json& json);
        };

        explicit Network(const std::shared_ptr<spdlog::logger>& logger);
        ~Network();

        std::future<HttpResponse> fetch(const HttpRequest &http_request);
    private:
        struct Request {
            std::string url;
            HttpMethod method;
            curl_slist *headers;
            std::string body;

            HttpResponse response;
            std::promise<HttpResponse> promise;
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
        bool stopping{false};
    };
}


