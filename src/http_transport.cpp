#include "codane/http_transport.hpp"

#include <cctype>
#include <charconv>
#include <stdexcept>
#include <utility>

namespace codane {
namespace {

bool contains_forbidden_header_char(std::string_view value) {
    return value.find('\r') != std::string_view::npos ||
           value.find('\n') != std::string_view::npos ||
           value.find('\0') != std::string_view::npos;
}

bool valid_header_name(std::string_view name) {
    if (name.empty()) return false;

    for (unsigned char c : name) {
        if (!(std::isalnum(c) ||
              c == '!' || c == '#' || c == '$' || c == '%' ||
              c == '&' || c == '\'' || c == '*' || c == '+' ||
              c == '-' || c == '.' || c == '^' || c == '_' ||
              c == '`' || c == '|' || c == '~')) {
            return false;
        }
    }

    return true;
}

std::string curl_quote(std::string_view input) {
    std::string out;
    out.reserve(input.size() + 2);
    out.push_back('"');

    for (unsigned char c : input) {
        switch (c) {
        case '\\':
            out += "\\\\";
            break;
        case '"':
            out += "\\\"";
            break;
        case '\n':
            out += "\\n";
            break;
        case '\r':
            out += "\\r";
            break;
        case '\t':
            out += "\\t";
            break;
        case '\0':
            throw std::invalid_argument("NUL in curl config value");
        default:
            out.push_back(static_cast<char>(c));
            break;
        }
    }

    out.push_back('"');
    return out;
}

HttpResponse failure(ChatFailureKind kind, std::string message) {
    HttpResponse response;
    response.failure = kind;
    response.error = std::move(message);
    return response;
}

bool is_https_url(std::string_view url) {
    constexpr std::string_view prefix = "https://";
    return url.size() > prefix.size() &&
           url.substr(0, prefix.size()) == prefix;
}

HttpResponse parse_curl_response(const ProcessResult& result) {
    if (result.stdout_truncated || result.stderr_truncated) {
        return failure(
            ChatFailureKind::InvalidResponse,
            "HTTP transport output was truncated"
        );
    }

    if (result.cancelled) {
        return failure(
            ChatFailureKind::Cancelled,
            "HTTP request cancelled"
        );
    }

    if (result.timed_out || result.exit_code == 28) {
        return failure(
            ChatFailureKind::Timeout,
            "HTTP request timed out"
        );
    }

    if (result.exit_code == 6 || result.exit_code == 7) {
        return failure(
            ChatFailureKind::Unavailable,
            "HTTP endpoint unavailable"
        );
    }

    if (result.exit_code != 0) {
        return failure(
            ChatFailureKind::Unavailable,
            "curl request failed"
        );
    }

    const auto separator = result.stdout_text.rfind('\n');
    if (separator == std::string::npos) {
        return failure(
            ChatFailureKind::InvalidResponse,
            "missing HTTP status framing"
        );
    }

    const std::string_view status_text(
        result.stdout_text.data() + separator + 1,
        result.stdout_text.size() - separator - 1
    );

    if (status_text.size() != 3 ||
        !std::isdigit(static_cast<unsigned char>(status_text[0])) ||
        !std::isdigit(static_cast<unsigned char>(status_text[1])) ||
        !std::isdigit(static_cast<unsigned char>(status_text[2]))) {
        return failure(
            ChatFailureKind::InvalidResponse,
            "invalid HTTP status framing"
        );
    }

    int status = 0;
    const auto [ptr, ec] = std::from_chars(
        status_text.data(),
        status_text.data() + status_text.size(),
        status
    );

    if (ec != std::errc{} ||
        ptr != status_text.data() + status_text.size()) {
        return failure(
            ChatFailureKind::InvalidResponse,
            "invalid HTTP status"
        );
    }

    HttpResponse response;
    response.status = status;
    response.body = result.stdout_text.substr(0, separator);
    return response;
}

} // namespace

HttpResponse FakeHttpTransport::perform(
    const HttpRequest& request,
    std::stop_token stop
) {
    if (stop.stop_requested()) {
        return failure(
            ChatFailureKind::Cancelled,
            "HTTP request cancelled"
        );
    }

    requests.push_back(request);

    if (responses.empty()) {
        return failure(
            ChatFailureKind::Unavailable,
            "fake HTTP response queue is empty"
        );
    }

    auto response = responses.front();
    responses.erase(responses.begin());
    return response;
}

CurlHttpTransport::CurlHttpTransport(Runner runner)
    : runner_(std::move(runner)) {
    if (!runner_) {
        throw std::invalid_argument("HTTP runner is required");
    }
}

HttpResponse CurlHttpTransport::perform(
    const HttpRequest& request,
    std::stop_token stop
) {
    if (stop.stop_requested()) {
        return failure(
            ChatFailureKind::Cancelled,
            "HTTP request cancelled"
        );
    }

    if (!is_https_url(request.url) ||
        request.url.find('\r') != std::string::npos ||
        request.url.find('\n') != std::string::npos ||
        request.url.find('\0') != std::string::npos) {
        return failure(
            ChatFailureKind::NotConfigured,
            "HTTPS URL required"
        );
    }

    for (const auto& [name, value] : request.headers) {
        if (!valid_header_name(name) ||
            contains_forbidden_header_char(value)) {
            return failure(
                ChatFailureKind::NotConfigured,
                "invalid HTTP header"
            );
        }
    }

    ProcessSpec spec;
    spec.argv = {"curl", "-q", "--config", "-"};
    spec.stop = stop;
    spec.timeout = request.timeout;
    spec.max_stdout = request.max_response;
    spec.max_stderr = 1u << 20;

    std::string config;

    config += "silent\n";
    config += "show-error\n";
    config += "url = ";
    config += curl_quote(request.url);
    config += "\n";

    config += "write-out = ";
    config += curl_quote("\\n%{http_code}");
    config += "\n";

    if (!request.body.empty()) {
        config += "request = \"POST\"\n";
        config += "data-binary = ";
        config += curl_quote(request.body);
        config += "\n";
    }

    for (const auto& [name, value] : request.headers) {
        config += "header = ";
        config += curl_quote(name + ": " + value);
        config += "\n";
    }

    spec.stdin_text = std::move(config);

    return parse_curl_response(runner_(spec));
}

} // namespace codane
