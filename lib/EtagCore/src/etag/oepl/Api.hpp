// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

// Independently authored HTTP interoperability. This is not OEPL's RF protocol.
namespace etag::oepl {
constexpr std::size_t maximumResponseBytes = 16384;
constexpr std::size_t maximumImageBytes = 512 * 1024;
constexpr std::uint32_t maximumPixels = 8 * 1024 * 1024;

struct Endpoint {
    std::array<char, 16> host{};
    std::uint16_t port = 80;
};

bool parseEndpoint(const std::string& url, Endpoint& endpoint);
bool normalizeMac(const std::string& input, std::array<char, 17>& output);
bool boundedJson(const char* text, std::size_t length, unsigned maximumDepth = 8);

struct ImageInfo { std::uint16_t width = 0, height = 0; };

// Reads JPEG headers, including a complete baseline SOF and SOS, and checks EOI.
// This validates the envelope/dimensions, not the compressed image entropy.
template<class File> bool inspectJpeg(File& file, ImageInfo& info) {
    info = {};
    const auto length = file.size();
    if (length < 32 || length > maximumImageBytes || !file.seek(length - 2) ||
        file.read() != 0xff || file.read() != 0xd9 || !file.seek(0) ||
        file.read() != 0xff || file.read() != 0xd8) return false;
    bool frame = false;
    std::array<int, 3> componentIds{};
    int frameComponents = 0;
    while (file.position() + 4 < length) {
        if (file.read() != 0xff) return false;
        int marker = file.read();
        while (marker == 0xff) marker = file.read();
        if (marker <= 0 || marker == 0xd8 || marker == 0xd9 ||
            (marker >= 0xd0 && marker <= 0xd7)) return false;
        const int hi = file.read(), lo = file.read();
        if (hi < 0 || lo < 0) return false;
        const std::size_t size = static_cast<std::size_t>((hi << 8) | lo);
        const auto start = file.position();
        if (size < 2 || start > length - 2 || size - 2 > length - 2 - start) return false;
        const auto end = start + size - 2;
        const bool sof = (marker >= 0xc0 && marker <= 0xcf &&
                          marker != 0xc4 && marker != 0xc8 && marker != 0xcc);
        if (sof) {
            if (marker != 0xc0 || frame || size < 11 || file.read() != 8) return false;
            const int h1 = file.read(), h2 = file.read(), w1 = file.read(), w2 = file.read();
            const int components = file.read();
            if (h1 < 0 || h2 < 0 || w1 < 0 || w2 < 0 ||
                (components != 1 && components != 3) || size != 8U + 3U * components) return false;
            const auto width = static_cast<std::uint16_t>((w1 << 8) | w2);
            const auto height = static_cast<std::uint16_t>((h1 << 8) | h2);
            if (!width || !height || width > 4096 || height > 4096 ||
                static_cast<std::uint32_t>(width) * height > maximumPixels) return false;
            for (int i = 0; i < components; ++i) {
                const int id = file.read(), sampling = file.read(), quantization = file.read();
                if (id < 0 || sampling < 0 || quantization < 0 || quantization > 3 ||
                    (sampling >> 4) < 1 || (sampling >> 4) > 4 ||
                    (sampling & 15) < 1 || (sampling & 15) > 4) return false;
                for (int j = 0; j < i; ++j) if (componentIds[j] == id) return false;
                componentIds[i] = id;
            }
            frameComponents = components;
            info = {width, height};
            frame = true;
        }
        if (marker == 0xda) {
            const int components = file.read();
            if (!frame || components != frameComponents ||
                size != 6U + 2U * components || end >= length - 2) return false;
            unsigned selected = 0;
            for (int i = 0; i < components; ++i) {
                const int id = file.read(), tables = file.read();
                if (id < 0 || tables < 0 || (tables >> 4) > 3 || (tables & 15) > 3) return false;
                int match = 0;
                while (match < frameComponents && componentIds[match] != id) ++match;
                if (match == frameComponents || (selected & (1U << match))) return false;
                selected |= 1U << match;
            }
            if (file.read() != 0 || file.read() != 63 || file.read() != 0) return false;
            return file.seek(0);
        }
        if (!file.seek(end)) return false;
    }
    return false;
}

class Body {
public:
    virtual ~Body() = default;
    virtual std::size_t size() const = 0;
    virtual std::size_t read(std::uint8_t* buffer, std::size_t capacity) = 0;
};

class Transport {
public:
    virtual ~Transport() = default;
    virtual bool connect(const char* host, std::uint16_t port) = 0;
    virtual std::size_t write(const std::uint8_t* bytes, std::size_t length) = 0;
    virtual int read() = 0; // nonblocking; -1 means no byte available
    virtual bool connected() = 0; // true while buffered bytes remain
    virtual void stop() = 0;
    virtual std::uint32_t now() = 0;
    virtual void idle() = 0;
    virtual bool cancelled() = 0;
};

enum class Error { none, invalidRequest, connect, timeout, cancelled, disconnected,
                   malformedResponse, oversizedResponse, sourceRead, httpStatus };
const char* errorText(Error error);

struct Response {
    int status = 0;
    std::size_t length = 0;
    std::array<char, maximumResponseBytes + 1> body{};
};

// Finite operation, no redirects/retries. Transport calls must also be bounded.
Error request(Transport& wire, const Endpoint& endpoint, const char* path,
              Response& response, Body* body = nullptr,
              const char* contentType = nullptr, std::uint32_t timeoutMs = 15000);
bool uploadAccepted(const Response& response);
std::string multipartHead(const char* mac, bool dither, const char* boundary);
std::string multipartTail(const char* boundary);
} // namespace etag::oepl
