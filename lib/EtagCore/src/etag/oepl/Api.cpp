// SPDX-License-Identifier: GPL-3.0-only
#include "Api.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>

namespace etag::oepl {
namespace {
bool decimal(const std::string& s, std::size_t& number) {
    if (s.empty()) return false;
    number = 0;
    for (char c : s) {
        if (c < '0' || c > '9' || number > (std::numeric_limits<std::size_t>::max() - 9) / 10)
            return false;
        number = number * 10 + static_cast<unsigned>(c - '0');
    }
    return true;
}
std::string trim(std::string s) {
    const auto first = s.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    return s.substr(first, s.find_last_not_of(" \t\r\n") - first + 1);
}
std::string lower(std::string s) {
    for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
struct Exchange {
    Transport& wire;
    std::uint32_t start, timeout;
    Error error = Error::none;
    bool ready() {
        if (wire.cancelled()) error = Error::cancelled;
        else if (wire.now() - start >= timeout) error = Error::timeout;
        return error == Error::none;
    }
    int byte() {
        while (ready()) {
            const int c = wire.read();
            if (c >= 0) return c;
            if (!wire.connected()) { error = Error::disconnected; return -1; }
            wire.idle();
        }
        return -1;
    }
    bool line(std::string& out, std::size_t limit) {
        out.clear();
        while (out.size() <= limit) {
            const int c = byte();
            if (c < 0) return false;
            if (c == '\r') {
                if (byte() == '\n') return true;
                if (error == Error::none) error = Error::malformedResponse;
                return false;
            }
            if (c == '\n' || c == 0 || (c < 32 && c != '\t')) {
                error = Error::malformedResponse; return false;
            }
            out += static_cast<char>(c);
        }
        error = Error::oversizedResponse;
        return false;
    }
    bool send(const std::uint8_t* bytes, std::size_t length) {
        std::size_t sent = 0;
        while (sent < length && ready()) {
            const auto n = wire.write(bytes + sent, length - sent);
            if (n > length - sent) { error = Error::disconnected; return false; }
            if (!n) {
                if (!wire.connected()) { error = Error::disconnected; return false; }
                wire.idle();
            }
            sent += n;
        }
        return sent == length && ready();
    }
    bool append(Response& response, std::size_t length) {
        if (length > maximumResponseBytes - response.length) {
            error = Error::oversizedResponse; return false;
        }
        for (std::size_t i = 0; i < length; ++i) {
            const int c = byte();
            if (c < 0) return false;
            response.body[response.length++] = static_cast<char>(c);
        }
        response.body[response.length] = 0;
        return true;
    }
};
struct Stop { Transport& wire; ~Stop() { wire.stop(); } };
} // namespace

bool parseEndpoint(const std::string& value, Endpoint& endpoint) {
    if (value.size() < 8 || value.size() > 40 || value.compare(0, 7, "http://") != 0) return false;
    std::string host = value.substr(7);
    if (!host.empty() && host.back() == '/') host.pop_back();
    const auto colon = host.find(':');
    std::size_t port = 80;
    if (colon != std::string::npos) {
        if (!decimal(host.substr(colon + 1), port) || !port || port > 65535) return false;
        host.resize(colon);
    }
    if (host.size() > 15) return false;
    std::size_t pos = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto end = host.find('.', pos);
        if ((end == std::string::npos) != (i == 3)) return false;
        const auto part = host.substr(pos, end == std::string::npos ? end : end - pos);
        std::size_t octet = 0;
        if (part.size() > 3 || (part.size() > 1 && part[0] == '0') ||
            !decimal(part, octet) || octet > 255) return false;
        pos = end + 1;
    }
    Endpoint result;
    std::memcpy(result.host.data(), host.c_str(), host.size() + 1);
    result.port = static_cast<std::uint16_t>(port);
    endpoint = result;
    return true;
}

bool normalizeMac(const std::string& input, std::array<char, 17>& output) {
    if (input.size() != 12 && input.size() != 16) return false;
    std::array<char, 17> mac{};
    mac.fill('0'); mac[16] = 0;
    const auto offset = 16 - input.size();
    for (std::size_t i = 0; i < input.size(); ++i) {
        const auto c = static_cast<unsigned char>(input[i]);
        if (!std::isxdigit(c)) return false;
        mac[i + offset] = static_cast<char>(std::toupper(c));
    }
    if (std::strcmp(mac.data(), "0000000000000000") == 0 ||
        std::strcmp(mac.data(), "FFFFFFFFFFFFFFFF") == 0) return false;
    output = mac;
    return true;
}

bool boundedJson(const char* text, std::size_t length, unsigned maximumDepth) {
    if (!text || !length || length > maximumResponseBytes || !maximumDepth) return false;
    unsigned depth = 0;
    bool quote = false, escape = false;
    for (std::size_t i = 0; i < length; ++i) {
        const unsigned char c = text[i];
        if (!c) return false;
        if (escape) { escape = false; continue; }
        if (quote && c == '\\') { escape = true; continue; }
        if (c == '"') { quote = !quote; continue; }
        if (quote) { if (c < 32) return false; continue; }
        if (c == '{' || c == '[') { if (++depth > maximumDepth) return false; }
        if (c == '}' || c == ']') { if (!depth) return false; --depth; }
    }
    return !quote && !escape && !depth;
}

Error request(Transport& wire, const Endpoint& endpoint, const char* path,
              Response& response, Body* body, const char* contentType, std::uint32_t timeoutMs) {
    response.status = 0; response.length = 0; response.body[0] = 0;
    Stop stop{wire};
    if (!path || path[0] != '/' || std::strlen(path) > 128 || !timeoutMs || timeoutMs > 120000 ||
        !endpoint.host[0] || endpoint.host.back() != 0 || !endpoint.port ||
        (body && (body->size() == 0 || body->size() > maximumImageBytes + 1024 || !contentType)))
        return Error::invalidRequest;
    for (const char* p = path; *p; ++p) if (*p <= ' ' || *p >= 127) return Error::invalidRequest;
    if (contentType) for (const char* p = contentType; *p; ++p)
        if (*p < ' ' || *p >= 127) return Error::invalidRequest;
    // Validate the endpoint even if the caller did not use parseEndpoint.
    Endpoint checked;
    if (!parseEndpoint(std::string("http://") + endpoint.host.data(), checked)) return Error::invalidRequest;
    Exchange exchange{wire, wire.now(), timeoutMs};
    if (!exchange.ready()) return exchange.error;
    if (!wire.connect(endpoint.host.data(), endpoint.port)) return Error::connect;
    if (!exchange.ready()) return exchange.error;
    std::string header = std::string(body ? "POST " : "GET ") + path + " HTTP/1.1\r\nHost: " +
        endpoint.host.data() + ":" + std::to_string(endpoint.port) +
        "\r\nConnection: close\r\nAccept: application/json, text/plain\r\nAccept-Encoding: identity\r\n";
    if (body) header += std::string("Content-Type: ") + contentType + "\r\nContent-Length: " +
        std::to_string(body->size()) + "\r\n";
    header += "\r\n";
    if (!exchange.send(reinterpret_cast<const std::uint8_t*>(header.data()), header.size())) return exchange.error;
    if (body) {
        std::array<std::uint8_t, 1024> bytes{};
        std::size_t remaining = body->size();
        while (remaining && exchange.ready()) {
            const auto n = body->read(bytes.data(), std::min(remaining, bytes.size()));
            if (!n || n > std::min(remaining, bytes.size())) return Error::sourceRead;
            if (!exchange.send(bytes.data(), n)) return exchange.error;
            remaining -= n;
        }
        if (!exchange.ready()) return exchange.error;
    }
    std::string line;
    if (!exchange.line(line, 128)) return exchange.error;
    if (line.size() < 12 || (line.compare(0, 9, "HTTP/1.1 ") && line.compare(0, 9, "HTTP/1.0 ")) ||
        !std::isdigit(static_cast<unsigned char>(line[9])) ||
        !std::isdigit(static_cast<unsigned char>(line[10])) ||
        !std::isdigit(static_cast<unsigned char>(line[11])) ||
        (line.size() > 12 && line[12] != ' ')) return Error::malformedResponse;
    response.status = (line[9] - '0') * 100 + (line[10] - '0') * 10 + line[11] - '0';
    if (response.status < 200 || response.status > 599) return Error::malformedResponse;
    bool hasLength = false, chunked = false;
    std::size_t contentLength = 0, headerSize = line.size();
    while (true) {
        if (!exchange.line(line, 1024)) return exchange.error;
        headerSize += line.size() + 2;
        if (headerSize > 4096) return Error::oversizedResponse;
        if (line.empty()) break;
        const auto colon = line.find(':');
        if (colon == std::string::npos || !colon || line[0] == ' ' || line[0] == '\t')
            return Error::malformedResponse;
        const auto key = lower(line.substr(0, colon)), value = lower(trim(line.substr(colon + 1)));
        if (key == "content-length") {
            if (hasLength || !decimal(value, contentLength)) return Error::malformedResponse;
            hasLength = true;
        } else if (key == "transfer-encoding") {
            if (chunked || value != "chunked") return Error::malformedResponse;
            chunked = true;
        } else if (key == "content-encoding" && value != "identity") return Error::malformedResponse;
    }
    if (hasLength && chunked) return Error::malformedResponse;
    if (chunked) {
        while (true) {
            if (!exchange.line(line, 128)) return exchange.error;
            const auto digits = line.substr(0, line.find(';'));
            if (digits.empty() || digits.size() > 8) return Error::malformedResponse;
            std::size_t count = 0;
            for (unsigned char c : digits) {
                if (!std::isxdigit(c)) return Error::malformedResponse;
                count = count * 16 + (c <= '9' ? c - '0' : std::tolower(c) - 'a' + 10);
            }
            if (!count) {
                std::size_t trailers = 0;
                do {
                    if (!exchange.line(line, 512)) return exchange.error;
                    trailers += line.size() + 2;
                    if (trailers > 4096) return Error::oversizedResponse;
                } while (!line.empty());
                break;
            }
            if (!exchange.append(response, count)) return exchange.error;
            if (!exchange.line(line, 0)) return exchange.error;
            if (!line.empty()) return Error::malformedResponse;
        }
    } else if (hasLength) {
        if (!exchange.append(response, contentLength)) return exchange.error;
    } else {
        while (exchange.ready()) {
            const int c = wire.read();
            if (c >= 0) {
                if (response.length == maximumResponseBytes) return Error::oversizedResponse;
                response.body[response.length++] = static_cast<char>(c);
            } else if (!wire.connected()) break;
            else wire.idle();
        }
        response.body[response.length] = 0;
        if (!exchange.ready()) return exchange.error;
    }
    return response.status >= 200 && response.status < 300 ? Error::none : Error::httpStatus;
}

bool uploadAccepted(const Response& response) {
    return response.status == 200 && trim(std::string(response.body.data(), response.length)) == "Ok, saved";
}
std::string multipartHead(const char* mac, bool dither, const char* boundary) {
    return std::string("--") + boundary + "\r\nContent-Disposition: form-data; name=\"mac\"\r\n\r\n" + mac +
        "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"dither\"\r\n\r\n" + (dither ? "1" : "0") +
        "\r\n--" + boundary + "\r\nContent-Disposition: form-data; name=\"file\"; filename=\"image.jpg\"\r\n" +
        "Content-Type: image/jpeg\r\n\r\n";
}
std::string multipartTail(const char* boundary) { return std::string("\r\n--") + boundary + "--\r\n"; }
const char* errorText(Error error) {
    switch (error) {
    case Error::none: return "OK";
    case Error::invalidRequest: return "Invalid AP request";
    case Error::connect: return "Cannot connect to AP";
    case Error::timeout: return "AP request timed out";
    case Error::cancelled: return "Cancelled; check AP status";
    case Error::disconnected: return "AP connection lost";
    case Error::malformedResponse: return "Invalid AP response";
    case Error::oversizedResponse: return "AP response too large";
    case Error::sourceRead: return "Image read failed";
    case Error::httpStatus: return "AP rejected request";
    }
    return "AP error";
}
} // namespace etag::oepl
