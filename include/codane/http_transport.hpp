#pragma once

#include "codane/chat_provider.hpp"
#include "codane/process.hpp"

#include <chrono>
#include <functional>
#include <map>
#include <stop_token>
#include <string>
#include <vector>

namespace codane {

struct HttpRequest {
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    std::chrono::milliseconds timeout{60000};
    std::size_t max_response = 8u << 20;
};

struct HttpResponse {
    int status = 0;
    std::string body;
    ChatFailureKind failure{};
    std::string error;
};

class HttpTransport {
public:
    virtual ~HttpTransport() = default;

    virtual HttpResponse perform(
        const HttpRequest& request,
        std::stop_token stop
    ) = 0;
};

class FakeHttpTransport final : public HttpTransport {
public:
    std::vector<HttpRequest> requests;
    std::vector<HttpResponse> responses;

    HttpResponse perform(
        const HttpRequest& request,
        std::stop_token stop
    ) override;
};

class CurlHttpTransport final : public HttpTransport {
public:
    using Runner = std::function<ProcessResult(const ProcessSpec&)>;

    explicit CurlHttpTransport(
        Runner runner = run_process
    );

    HttpResponse perform(
        const HttpRequest& request,
        std::stop_token stop
    ) override;

private:
    Runner runner_;
};

} // namespace codane
