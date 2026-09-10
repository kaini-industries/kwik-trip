// SPDX-License-Identifier: GPL-3.0-only
#include "etag/oepl/Api.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

using namespace etag::oepl;

namespace {
void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class FakeTransport final : public Transport {
public:
    std::string input, output;
    std::size_t cursor = 0, writeLimit = 7;
    std::uint32_t clock = 0, cancelAt = std::numeric_limits<std::uint32_t>::max();
    unsigned stops = 0, connects = 0;
    bool connectResult = true, open = false, holdOpen = false, stalledWrite = false;
    bool connect(const char*, std::uint16_t) override { ++connects; open = connectResult; return open; }
    std::size_t write(const std::uint8_t* bytes, std::size_t length) override {
        if (!open || stalledWrite) return 0;
        const auto count = std::min(length, writeLimit);
        output.append(reinterpret_cast<const char*>(bytes), count);
        return count;
    }
    int read() override { return cursor < input.size() ? static_cast<unsigned char>(input[cursor++]) : -1; }
    bool connected() override { return open && (cursor < input.size() || holdOpen); }
    void stop() override { ++stops; open = false; }
    std::uint32_t now() override { return clock; }
    void idle() override { ++clock; }
    bool cancelled() override { return clock >= cancelAt; }
};

class StringBody final : public Body {
public:
    std::string value;
    std::size_t advertised, cursor = 0;
    bool overReport = false;
    explicit StringBody(std::string text) : value(std::move(text)), advertised(value.size()) {}
    std::size_t size() const override { return advertised; }
    std::size_t read(std::uint8_t* bytes, std::size_t capacity) override {
        if (overReport) return capacity + 1;
        const auto count = std::min({capacity, value.size() - cursor, std::size_t{3}});
        std::memcpy(bytes, value.data() + cursor, count);
        cursor += count;
        return count;
    }
};

Endpoint endpoint() {
    Endpoint result;
    check(parseEndpoint("http://192.168.1.42:8080", result), "test endpoint parses");
    return result;
}

std::string reply(const std::string& body, int status = 200) {
    return "HTTP/1.1 " + std::to_string(status) + " Result\r\nContent-Length: " +
           std::to_string(body.size()) + "\r\n\r\n" + body;
}

void expectResponse(const std::string& input, Error wanted, const char* message) {
    FakeTransport wire;
    wire.input = input;
    Response response;
    check(request(wire, endpoint(), "/get_db", response) == wanted, message);
    check(wire.stops == 1 && !wire.open, "every exchange closes its transport");
}

void addressTests() {
    Endpoint ep;
    check(parseEndpoint("http://127.0.0.1:65535/", ep) && ep.port == 65535, "IPv4 and explicit port");
    for (const auto* input : {"https://192.168.1.1", "http://localhost", "http://1.2.3", "http://1.2.3.256",
                              "http://1.02.3.4", "http://1.2.3.4:0", "http://1.2.3.4:65536",
                              "http://1.2.3.4/path", "http://1.2.3.4\r\nX:1", "http://[::1]"})
        check(!parseEndpoint(input, ep), "reject malformed or unsupported endpoint");
    std::array<char, 17> mac{};
    check(normalizeMac("aabbccddeeff", mac) && std::string(mac.data()) == "0000AABBCCDDEEFF", "normalize BLE MAC");
    check(normalizeMac("0123456789abcdef", mac) && std::string(mac.data()) == "0123456789ABCDEF", "normalize tag MAC");
    for (const auto* input : {"0000000000000000", "FFFFFFFFFFFFFFFF", "00112233445566GG", "00:11:22:33:44:55"})
        check(!normalizeMac(input, mac), "reject malformed or sentinel MAC");
    const std::string quotedJson = "{\"a\":[\"{\\\"}\"]}";
    check(boundedJson(quotedJson.data(), quotedJson.size()), "JSON guard handles quoted brackets and escapes");
    check(!boundedJson("[[[]]]", 6, 2), "JSON depth limit");
    check(!boundedJson("{\"a\":\"x", 7), "JSON unterminated string");
    const char nul[] = {'{', 0, '}'};
    check(!boundedJson(nul, sizeof(nul)), "JSON embedded NUL");
}

void httpTests() {
    FakeTransport wire;
    wire.input = reply("{\"tags\":[]}");
    Response response;
    check(request(wire, endpoint(), "/get_db", response) == Error::none, "length-delimited GET");
    check(std::string(response.body.data(), response.length) == "{\"tags\":[]}", "response bytes preserved");
    check(wire.output.find("GET /get_db HTTP/1.1\r\nHost: 192.168.1.42:8080\r\n") == 0, "request line and host");
    check(wire.stops == 1, "successful GET closes connection");

    FakeTransport chunk;
    chunk.input = "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\n4;name=value\r\nOk, \r\n5\r\nsaved\r\n0\r\nX-Info: value\r\n\r\n";
    check(request(chunk, endpoint(), "/get_db", response) == Error::none && uploadAccepted(response), "chunk extensions and trailers");
    expectResponse("HTTP/1.0 200 OK\r\n\r\nplain", Error::none, "connection-close response");
    expectResponse(reply("rejected", 409), Error::httpStatus, "HTTP error retained");
    expectResponse("HTTP/1.1 200 OK\r\nContent-Length: 2\r\nContent-Length: 2\r\n\r\n{}", Error::malformedResponse, "duplicate length rejected");
    expectResponse("HTTP/1.1 200 OK\r\nContent-Length: 0\r\nTransfer-Encoding: chunked\r\n\r\n0\r\n\r\n", Error::malformedResponse, "conflicting framing rejected");
    expectResponse("HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 0\r\n\r\n", Error::malformedResponse, "unsupported compression rejected");
    expectResponse("HTTP/1.1 200 OK\n\n", Error::malformedResponse, "bare LF rejected");
    expectResponse("HTTP/1.1 \xff" "00 OK\r\n\r\n", Error::malformedResponse, "high-bit status digit rejected");
    expectResponse("HTTP/1.1 200 OK\r\nContent-Length: 5\r\n\r\nab", Error::disconnected, "truncated body rejected");
    expectResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nZ\r\n", Error::malformedResponse, "invalid chunk size rejected");
    expectResponse("HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nFFFFFFFF\r\n", Error::oversizedResponse, "huge chunk rejected before reading");
    expectResponse("HTTP/1.1 200 OK\r\nContent-Length: 16385\r\n\r\n", Error::oversizedResponse, "response limit checked before body");
    expectResponse(reply(std::string(maximumResponseBytes, 'x')), Error::none, "exact response bound accepted");
    expectResponse("HTTP/1.1 200 OK\r\nX-Long: " + std::string(1100, 'x') + "\r\n\r\n", Error::oversizedResponse, "individual header limit");
    std::string headers = "HTTP/1.1 200 OK\r\n";
    for (int i = 0; i < 100; ++i) headers += "X-Pad: " + std::string(45, 'x') + "\r\n";
    expectResponse(headers + "\r\n", Error::oversizedResponse, "aggregate header limit");

    FakeTransport empty;
    empty.input = reply("");
    check(request(empty, endpoint(), "/imgupload", response) == Error::none && !uploadAccepted(response), "bare 200 is not image acceptance");
    FakeTransport nonSuccess;
    nonSuccess.input = reply("Ok, saved", 400);
    check(request(nonSuccess, endpoint(), "/imgupload", response) == Error::httpStatus && !uploadAccepted(response), "success text cannot override status");
}

void failureAndBodyTests() {
    Response response;
    FakeTransport noConnection;
    noConnection.connectResult = false;
    check(request(noConnection, endpoint(), "/get_db", response) == Error::connect && noConnection.stops == 1, "connect failure closes transport");
    FakeTransport timeout;
    timeout.holdOpen = true;
    check(request(timeout, endpoint(), "/get_db", response, nullptr, nullptr, 5) == Error::timeout && timeout.clock == 5, "read deadline finite");
    FakeTransport blockedWrite;
    blockedWrite.holdOpen = true; blockedWrite.stalledWrite = true;
    check(request(blockedWrite, endpoint(), "/get_db", response, nullptr, nullptr, 4) == Error::timeout, "write deadline finite");
    FakeTransport cancelled;
    cancelled.holdOpen = true; cancelled.cancelAt = 2;
    check(request(cancelled, endpoint(), "/get_db", response) == Error::cancelled && cancelled.stops == 1, "cancel stalled response");
    FakeTransport beforeConnect;
    beforeConnect.cancelAt = 0;
    check(request(beforeConnect, endpoint(), "/get_db", response) == Error::cancelled && beforeConnect.connects == 0, "cancel before network operation");
    FakeTransport invalid;
    check(request(invalid, endpoint(), "/get_db\r\nInjected: yes", response) == Error::invalidRequest && invalid.connects == 0, "reject injected request path");

    const auto head = multipartHead("0011223344556677", true, "etag-test-boundary");
    StringBody body(head + "jpeg-bytes" + multipartTail("etag-test-boundary"));
    FakeTransport post;
    post.input = reply("Ok, saved");
    check(request(post, endpoint(), "/imgupload", response, &body, "multipart/form-data; boundary=etag-test-boundary") == Error::none && uploadAccepted(response), "multipart POST accepts partial source reads and network writes");
    const auto payload = post.output.find("\r\n\r\n");
    check(payload != std::string::npos && post.output.substr(payload + 4) == body.value, "POST streams exact body without loss");
    check(head.find("name=\"mac\"") < head.find("name=\"file\""), "MAC precedes file callback");
    check(post.output.find("Content-Length: " + std::to_string(body.value.size())) != std::string::npos, "POST byte length includes framing");

    StringBody shortBody("short"); shortBody.advertised = 20;
    FakeTransport shortWire; shortWire.input = reply("Ok, saved");
    check(request(shortWire, endpoint(), "/imgupload", response, &shortBody, "image/jpeg") == Error::sourceRead && shortWire.stops == 1, "short source aborts before response success");
    StringBody badBody("x"); badBody.overReport = true;
    FakeTransport badWire; badWire.input = reply("Ok, saved");
    check(request(badWire, endpoint(), "/imgupload", response, &badBody, "image/jpeg") == Error::sourceRead, "source cannot exceed supplied buffer");
}

class FakeFile {
public:
    std::vector<std::uint8_t> bytes;
    std::size_t cursor = 0, failReadAt = std::numeric_limits<std::size_t>::max();
    explicit FakeFile(std::vector<std::uint8_t> data) : bytes(std::move(data)) {}
    std::size_t size() const { return bytes.size(); }
    std::size_t position() const { return cursor; }
    bool seek(std::size_t position) { if (position > bytes.size()) return false; cursor = position; return true; }
    int read() { return cursor < bytes.size() && cursor != failReadAt ? bytes[cursor++] : -1; }
};

void segment(std::vector<std::uint8_t>& bytes, std::uint8_t marker, std::vector<std::uint8_t> payload) {
    const auto length = payload.size() + 2;
    bytes.insert(bytes.end(), {0xff, marker, static_cast<std::uint8_t>(length >> 8), static_cast<std::uint8_t>(length)});
    bytes.insert(bytes.end(), payload.begin(), payload.end());
}
std::vector<std::uint8_t> jpeg(unsigned components = 3) {
    std::vector<std::uint8_t> bytes{0xff, 0xd8};
    segment(bytes, 0xe0, std::vector<std::uint8_t>(10, 0));
    std::vector<std::uint8_t> frame{8, 0, 128, 1, 40, static_cast<std::uint8_t>(components)};
    std::vector<std::uint8_t> scan{static_cast<std::uint8_t>(components)};
    for (unsigned i = 1; i <= components; ++i) {
        frame.insert(frame.end(), {static_cast<std::uint8_t>(i), 0x11, 0});
        scan.insert(scan.end(), {static_cast<std::uint8_t>(i), 0});
    }
    scan.insert(scan.end(), {0, 63, 0});
    segment(bytes, 0xc0, frame);
    segment(bytes, 0xda, scan);
    bytes.insert(bytes.end(), {0x42, 0xff, 0xd9});
    return bytes;
}
std::size_t markerOffset(const std::vector<std::uint8_t>& bytes, std::uint8_t marker) {
    for (std::size_t i = 0; i + 1 < bytes.size(); ++i) if (bytes[i] == 0xff && bytes[i + 1] == marker) return i;
    throw std::runtime_error("missing fixture marker");
}
void jpegTests() {
    ImageInfo info;
    for (unsigned components : {1U, 3U}) {
        FakeFile file(jpeg(components));
        check(inspectJpeg(file, info) && info.width == 296 && info.height == 128 && file.position() == 0, "baseline JPEG header accepted and rewound");
    }
    auto bytes = jpeg();
    const auto sof = markerOffset(bytes, 0xc0), sos = markerOffset(bytes, 0xda);
    const std::vector<std::pair<std::size_t, std::uint8_t>> invalid{
        {sof + 1, 0xc2}, {sof + 4, 12}, {sof + 7, 0}, {sof + 11, 0},
        {sof + 12, 4}, {sof + 13, 1}, {sos + 5, 9}, {sos + 7, 1},
        {sos + 6, 0x44}, {sos + 11, 1}, {sos + 12, 62}, {sos + 13, 1}};
    for (const auto& mutation : invalid) {
        auto altered = bytes;
        altered[mutation.first] = mutation.second;
        // Setting the high width byte to zero leaves a valid smaller width, so also clear its low byte.
        if (mutation.first == sof + 7) altered[sof + 8] = 0;
        FakeFile file(std::move(altered));
        check(!inspectJpeg(file, info), "reject invalid baseline JPEG frame/scan descriptors");
    }
    FakeFile truncated(bytes); truncated.bytes.pop_back();
    check(!inspectJpeg(truncated, info), "JPEG requires terminal EOI");
    FakeFile readFailure(bytes); readFailure.failReadAt = sof + 11;
    check(!inspectJpeg(readFailure, info), "JPEG descriptor read failure rejected");
    FakeFile oversized(std::vector<std::uint8_t>(maximumImageBytes + 1, 0));
    check(!inspectJpeg(oversized, info), "JPEG size bound before reads");
}
} // namespace

int main() {
    try {
        addressTests();
        httpTests();
        failureAndBodyTests();
        jpegTests();
        std::cout << "OEPL core: endpoint, HTTP framing, deadlines, multipart, and JPEG checks passed\n";
    } catch (const std::exception& error) {
        std::cerr << "OEPL core failure: " << error.what() << '\n';
        return 1;
    }
}
