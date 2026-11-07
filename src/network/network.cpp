#include "network.h"

namespace YomitanMPV {
    void network_pre_init() {
        if (curl_global_init(CURL_GLOBAL_ALL) != CURLE_OK)
            throw std::runtime_error("curl_global_init() failed");
    }

    void network_destroy() {
        curl_global_cleanup();
    }

    std::string Network::HttpRequest::url() const {
        return _url;
    }

    Network::HttpRequest &Network::HttpRequest::url(const std::string &url) {
        _url = url;

        return *this;
    }

    Network::HttpMethod Network::HttpRequest::method() const {
        return _method;
    }

    Network::HttpRequest &Network::HttpRequest::method(const HttpMethod method) {
        _method = method;

        return *this;
    }

    std::string Network::HttpRequest::body() const {
        return _body;
    }

    curl_slist *Network::HttpRequest::headers() const {
        return _headers;
    }

    Network::HttpRequest &Network::HttpRequest::header(const std::string& key, const std::string &value) {
        _headers = curl_slist_append(_headers, (key + ": " + value).c_str());

        return *this;
    }

    Network::HttpRequest &Network::HttpRequest::body(const std::string &text) {
        _body = text;
        header("Content-Type", "text/plain");

        return *this;
    }

    Network::HttpRequest &Network::HttpRequest::body(const nlohmann::json &json) {
        _body = json.dump();
        header("Content-Type", "application/json");

        return *this;
    }

    Network::Network(const std::shared_ptr<spdlog::logger> &logger) {
        this->logger = logger;

        this->curl_multi = curl_multi_init();

        if (!curl_multi) {
            throw std::runtime_error("curl_multi_init failed");
        }

        curl_multi_setopt(curl_multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 16L);
        curl_multi_setopt(curl_multi, CURLMOPT_MAX_HOST_CONNECTIONS, 6L);

        try {
            worker = std::thread(&Network::run, this);
        } catch (...) {
            curl_multi_cleanup(curl_multi);

            throw;
        }
    }

    std::future<Network::HttpResponse> Network::fetch(const HttpRequest &http_request) {
        auto request = std::make_unique<Request>();
        request->url = http_request.url();
        request->method = http_request.method();
        request->headers = http_request.headers();
        request->body = http_request.body();

        auto future = request->promise.get_future();

        {
            std::lock_guard lock(mutex);

            if (stopping) {
                throw std::runtime_error("Network is stopping");
            }

            pending.push(std::move(request));

            curl_multi_wakeup(curl_multi);
        }

        return future;
    }

    size_t Network::writeCallback(
        const char* data,
        const size_t size,
        const size_t nmemb,
        void* userdata
    ) {
        if (size != 0 && nmemb > std::numeric_limits<size_t>::max() / size) {
            return 0;
        }

        const size_t bytes = size * nmemb;
        auto* body = static_cast<std::string*>(userdata);

        try {
            body->append(data, bytes);
        } catch (...) {
            return 0;
        }

        return bytes;
    }

    void Network::run() {
        while (true) {
            std::queue<std::unique_ptr<Request>> requests;
            bool shouldStop = false;

            {
                std::lock_guard lock(mutex);

                shouldStop = stopping;
                requests.swap(pending);
            }

            if (shouldStop) {
                while (!requests.empty()) {
                    const auto request = std::move(requests.front());
                    requests.pop();

                    request->response.curl_status = CURLE_ABORTED_BY_CALLBACK;

                    request->promise.set_value(std::move(request->response));
                }

                break;
            }

            while (!requests.empty()) {
                auto request = std::move(requests.front());
                requests.pop();

                addRequest(std::move(request));
            }

            int running = 0;

            CURLMcode result = curl_multi_perform(curl_multi, &running);

            if (result != CURLM_OK) {
                if (logger)
                    logger->error("curl_multi_perform: {}", curl_multi_strerror(result));

                break;
            }

            processCompleted();

            result = curl_multi_poll(
                curl_multi,
                nullptr,
                0,
                1000,
                nullptr
            );

            if (result != CURLM_OK) {
                if (logger)
                    logger->error("curl_multi_poll: {}", curl_multi_strerror(result));

                break;
            }
        }

        std::queue<std::unique_ptr<Request>> remaining;

        {
            std::lock_guard lock(mutex);
            stopping = true;
            remaining.swap(pending);
        }

        while (!remaining.empty()) {
            const auto request = std::move(remaining.front());
            remaining.pop();

            request->response.curl_status = CURLE_ABORTED_BY_CALLBACK;

            request->promise.set_value(std::move(request->response));
        }

        for (auto& [easy, request] : active) {
            curl_multi_remove_handle(curl_multi, easy);
            curl_easy_cleanup(easy);

            request->response.curl_status = CURLE_ABORTED_BY_CALLBACK;

            request->promise.set_value(std::move(request->response));
        }

        active.clear();
    }

    void Network::processCompleted() {
        int messagesLeft = 0;
        const CURLMsg* message = nullptr;

        while ((message = curl_multi_info_read(curl_multi, &messagesLeft))) {

            if (message->msg != CURLMSG_DONE)
                continue;

            CURL* easy = message->easy_handle;

            auto it = active.find(easy);

            if (it == active.end())
                continue;

            const auto request = std::move(it->second);
            active.erase(it);

            request->response.curl_status = message->data.result;

            curl_easy_getinfo(easy, CURLINFO_RESPONSE_CODE, &request->response.status_code);

            curl_multi_remove_handle(curl_multi, easy);
            curl_easy_cleanup(easy);

            if (logger && !request->response.is_ok()) {
                logger->warn(
                "HTTP request failed: {}, CURL={}, HTTP={}",
                    request->url,
                    curl_easy_strerror(request->response.curl_status),
                    request->response.status_code
                );
            }

            request->promise.set_value(std::move(request->response));
        }
    }

    void Network::addRequest(std::unique_ptr<Request> request) {
        CURL* easy = curl_easy_init();

        if (!easy) {
            request->response.curl_status = CURLE_FAILED_INIT;
            request->promise.set_value(std::move(request->response));
            return;
        }

        curl_easy_setopt(easy, CURLOPT_URL, request->url.c_str());

        switch (request->method) {
            case GET:
                curl_easy_setopt(easy, CURLOPT_HTTPGET, 1L);
            case POST:
                curl_easy_setopt(easy, CURLOPT_POST, 1L);
        }
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, request->headers);
        curl_easy_setopt(easy, CURLOPT_POSTFIELDS, request->body.c_str());
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, &Network::writeCallback);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, &request->response.body);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_MAXREDIRS, 5L);
        curl_easy_setopt(easy, CURLOPT_CONNECTTIMEOUT_MS, 5000L);
        curl_easy_setopt(easy, CURLOPT_TIMEOUT_MS, 15000L);
        curl_easy_setopt(easy, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYHOST, 2L);
        curl_easy_setopt(easy, CURLOPT_PROTOCOLS_STR, "http,https");
        curl_easy_setopt(easy, CURLOPT_REDIR_PROTOCOLS_STR, "http,https");

        if (const CURLMcode result = curl_multi_add_handle(curl_multi, easy); result != CURLM_OK) {
            if (logger) {
                logger->error("curl_multi_add_handle: {}", curl_multi_strerror(result));
            }

            request->response.status_code = CURLE_FAILED_INIT;

            curl_easy_cleanup(easy);

            request->promise.set_value(std::move(request->response));
            return;
        }

        active.emplace(easy, std::move(request));
    }

    Network::~Network() {
        {
            std::lock_guard lock(mutex);
            stopping = true;

            curl_multi_wakeup(curl_multi);
        }

        if (worker.joinable())
            worker.join();

        curl_multi_cleanup(curl_multi);
    }
}
