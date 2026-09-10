// SPDX-License-Identifier: GPL-3.0-only
#ifdef ETAG_CARDPUTER_ADV
#include "OeplMode.hpp"
#include "../sd_card.h"
#include <M5Cardputer.h>
#include <SD.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_heap_caps.h>
#include <esp_system.h>
#include <mbedtls/sha256.h>
#include <lwip/sockets.h>
#include <algorithm>
#include <cmath>
#include <cerrno>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <memory>

namespace etag_host {
namespace {
using namespace etag::oepl;
using Json = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;
constexpr const char* configPath = "/etag/oepl.local.json";

bool uniqueFields(const cJSON* object) {
    if (!cJSON_IsObject(object)) return false;
    for (const cJSON* a = object->child; a; a = a->next) {
        if (!a->string) return false;
        for (const cJSON* b = a->next; b; b = b->next)
            if (b->string && std::strcmp(a->string, b->string) == 0) return false;
    }
    return true;
}
Json parse(const char* bytes, std::size_t length) {
    if (!boundedJson(bytes, length)) return {nullptr, cJSON_Delete};
    Json json(cJSON_ParseWithOpts(bytes, nullptr, true), cJSON_Delete);
    if (!uniqueFields(json.get()) || cJSON_GetObjectItemCaseSensitive(json.get(), "error")) json.reset();
    return json;
}
const char* stringField(const cJSON* json, const char* key) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, key);
    return cJSON_IsString(value) ? value->valuestring : nullptr;
}
int integerField(const cJSON* json, const char* key, int minimum, int maximum, int fallback) {
    const cJSON* value = cJSON_GetObjectItemCaseSensitive(json, key);
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) ||
        std::floor(value->valuedouble) != value->valuedouble ||
        value->valuedouble < minimum || value->valuedouble > maximum) return fallback;
    return static_cast<int>(value->valuedouble);
}
template<std::size_t N> bool copyField(const cJSON* json, const char* key, std::array<char, N>& output,
                                     bool empty = false) {
    const char* value = stringField(json, key);
    if (!value || (!empty && !*value) || std::strlen(value) >= N) return false;
    for (const char* p = value; *p; ++p) if (static_cast<unsigned char>(*p) < 32) return false;
    std::strcpy(output.data(), value);
    return true;
}

class Socket final : public Transport {
public:
    explicit Socket(std::atomic<bool>& cancel) : cancel_(cancel) {}
    bool connect(const char* host, std::uint16_t port) override {
        IPAddress ip;
        if (!ip.fromString(host)) return false;
        client_.setTimeout(2); // WiFiClient uses seconds; connect has its own millisecond limit.
        return client_.connect(ip, port, 3000);
    }
    std::size_t write(const std::uint8_t* bytes, std::size_t length) override {
        // WiFiClient::write retries internally and can hide partial-progress
        // stalls from our total deadline. One nonblocking send keeps ownership
        // of retry/cancel/timeout decisions in the portable exchange.
        const int sent = ::send(client_.fd(), bytes, length, MSG_DONTWAIT);
        if (sent >= 0) return static_cast<std::size_t>(sent);
        if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) client_.stop();
        return 0;
    }
    int read() override { return client_.available() ? client_.read() : -1; }
    bool connected() override { return client_.available() || client_.connected(); }
    void stop() override { client_.stop(); }
    std::uint32_t now() override { return millis(); }
    void idle() override { delay(1); }
    bool cancelled() override { return cancel_.load(); }
private:
    WiFiClient client_;
    std::atomic<bool>& cancel_;
};

class UploadBody final : public Body {
public:
    UploadBody(File& file, std::string head, std::string tail, std::atomic<unsigned>& progress)
        : file_(file), head_(std::move(head)), tail_(std::move(tail)), progress_(progress), fileSize_(file.size()) {}
    std::size_t size() const override { return head_.size() + fileSize_ + tail_.size(); }
    std::size_t read(std::uint8_t* out, std::size_t capacity) override {
        std::size_t count = 0;
        while (count < capacity && offset_ < size()) {
            std::size_t n;
            if (offset_ < head_.size()) {
                n = std::min(capacity - count, head_.size() - offset_);
                std::memcpy(out + count, head_.data() + offset_, n);
            } else if (offset_ < head_.size() + fileSize_) {
                const auto wanted = std::min(capacity - count, head_.size() + fileSize_ - offset_);
                n = file_.read(out + count, wanted);
                if (n != wanted) return 0;
            } else {
                const auto tailOffset = offset_ - head_.size() - fileSize_;
                n = std::min(capacity - count, tail_.size() - tailOffset);
                std::memcpy(out + count, tail_.data() + tailOffset, n);
            }
            count += n; offset_ += n;
        }
        progress_.store(static_cast<unsigned>(offset_ * 100 / size()));
        return count;
    }
private:
    File& file_;
    const std::string head_, tail_;
    std::atomic<unsigned>& progress_;
    const std::size_t fileSize_;
    std::size_t offset_ = 0;
};
} // namespace

void OeplMode::fail(const char* value) { std::snprintf(message_.data(), message_.size(), "%s", value); }

void OeplMode::loadConfig() {
    configured_ = false;
    ssid_.fill(0); password_.fill(0); endpoint_ = {};
    if (!mountSd()) { fail("Insert SD with OEPL settings"); return; }
    File file = SD.open(configPath, FILE_READ);
    if (!file || file.isDirectory() || file.size() == 0 || file.size() > 1024) {
        fail("Missing/invalid OEPL settings"); return;
    }
    std::array<char, 1025> bytes{};
    const auto size = file.size();
    if (file.read(reinterpret_cast<std::uint8_t*>(bytes.data()), size) != size) {
        fail("Cannot read OEPL settings"); return;
    }
    auto config = parse(bytes.data(), size);
    const char* ap = stringField(config.get(), "ap");
    if (!config || integerField(config.get(), "schema_version", 1, 1, 0) != 1 ||
        !copyField(config.get(), "ssid", ssid_) || !copyField(config.get(), "password", password_, true) ||
        !ap || !parseEndpoint(ap, endpoint_)) {
        password_.fill(0); fail("Invalid settings; see OEPL guide"); return;
    }
    configured_ = true;
    fail("Connect to OpenEPaperLink AP");
}

void OeplMode::begin() {
    exit_ = false; leaveAfterWork_ = false; waiting_ = false; cancel_.store(false);
    view_ = View::setup; tagCount_ = 0; selected_ = {}; previewReady_ = false;
    loadConfig(); draw();
}

void OeplMode::stop() {
    if (busy_.load()) return;
    WiFi.disconnect(true, false);
    WiFi.mode(WIFI_OFF);
    password_.fill(0); ssid_.fill(0); selected_ = {};
    for (auto& tag : tags_) tag = {};
    tagCount_ = 0;
    response_.body.fill(0); response_.length = 0;
}

void OeplMode::launch(Job job) {
    if (busy_.load()) return;
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < 24576 || ESP.getFreeHeap() < 70000) {
        fail("Low memory; reboot after BLE"); returnView_ = view_; view_ = View::result; draw(); return;
    }
    job_ = job; returnView_ = view_; view_ = View::working;
    cancel_.store(false); progress_.store(0); busy_.store(true); waiting_ = true;
    okay_ = false; previewReady_ = false; draw();
    if (xTaskCreate(task, "etag-oepl", 12288, this, 1, nullptr) != pdPASS) {
        busy_.store(false); waiting_ = false; fail("Cannot start OEPL worker"); view_ = View::result; draw();
    }
}

void OeplMode::task(void* argument) {
    auto& self = *static_cast<OeplMode*>(argument);
    self.okay_ = self.work();
    // The release/acquire handoff publishes records/message to the UI. After
    // clearing busy this task must never access the mode or its buffers again.
    self.busy_.store(false, std::memory_order_release);
    vTaskDelete(nullptr);
}

bool OeplMode::fetch(const char* path) {
    Socket wire(cancel_);
    const auto error = request(wire, endpoint_, path, response_);
    if (error != Error::none) { fail(errorText(error)); return false; }
    return true;
}

bool OeplMode::fetchTags() {
    char path[40]; std::snprintf(path, sizeof(path), "/get_db?pos=%u", offset_);
    if (!fetch(path)) return false;
    auto json = parse(response_.body.data(), response_.length);
    const auto* list = cJSON_GetObjectItemCaseSensitive(json.get(), "tags");
    if (!json || !cJSON_IsArray(list) || cJSON_GetArraySize(list) > static_cast<int>(tags_.size())) {
        fail("Invalid/large tag page; use Studio"); return false;
    }
    const auto* continuation = cJSON_GetObjectItemCaseSensitive(json.get(), "continu");
    const int next = integerField(json.get(), "continu", 0, 255, -1);
    if (continuation && (next < 0 || next <= static_cast<int>(offset_))) {
        fail("Invalid AP pagination"); return false;
    }
    std::size_t count = 0;
    std::array<Tag, 32> freshTags{};
    for (const auto* entry = list->child; entry; entry = entry->next) {
        Tag tag;
        const char* mac = stringField(entry, "mac");
        if (!uniqueFields(entry) || !mac || !normalizeMac(mac, tag.mac)) {
            fail("Invalid tag record from AP"); return false;
        }
        for (std::size_t i = 0; i < count; ++i) if (freshTags[i].mac == tag.mac) {
            fail("Duplicate AP tag"); return false;
        }
        const char* alias = stringField(entry, "alias");
        if (alias) {
            std::snprintf(tag.alias.data(), tag.alias.size(), "%s", alias);
            for (auto& c : tag.alias) if (static_cast<unsigned char>(c) < 32 && c) c = ' ';
        }
        tag.hwType = integerField(entry, "hwType", 0, 255, -1);
        freshTags[count++] = tag;
    }
    tags_ = freshTags;
    tagCount_ = count; listPage_ = 0; nextOffset_ = next;
    return true;
}

bool OeplMode::fetchSelected() {
    char path[48]; std::snprintf(path, sizeof(path), "/get_db?mac=%s", selected_.mac.data());
    if (!fetch(path)) return false;
    auto json = parse(response_.body.data(), response_.length);
    const auto* list = cJSON_GetObjectItemCaseSensitive(json.get(), "tags");
    if (!json || !cJSON_IsArray(list) || cJSON_GetArraySize(list) != 1) {
        fail("Tag is no longer known to AP"); return false;
    }
    const auto* tag = list->child;
    std::array<char, 17> mac{};
    const char* macText = stringField(tag, "mac");
    if (!uniqueFields(tag) || !macText || !normalizeMac(macText, mac) || mac != selected_.mac) {
        fail("AP returned a different tag"); return false;
    }
    selected_.hwType = integerField(tag, "hwType", 0, 255, -1);
    selected_.batteryMv = integerField(tag, "batteryMv", 0, 65535, -1);
    selected_.rssi = integerField(tag, "RSSI", -127, 0, -129);
    selected_.pending = integerField(tag, "pending", 0, 65535, -1);
    selected_.width = selected_.height = 0;
    if (selected_.hwType < 0) { fail("AP tag type is unknown"); return false; }
    std::snprintf(path, sizeof(path), "/tagtypes/%02X.json", selected_.hwType);
    if (!fetch(path)) return false;
    auto type = parse(response_.body.data(), response_.length);
    const int width = integerField(type.get(), "width", 1, 4096, 0);
    const int height = integerField(type.get(), "height", 1, 4096, 0);
    if (!type || !width || !height || static_cast<unsigned>(width * height) > maximumPixels) {
        fail("AP has no valid display size"); return false;
    }
    selected_.width = static_cast<std::uint16_t>(width);
    selected_.height = static_cast<std::uint16_t>(height);
    return true;
}

bool OeplMode::inspectImage(std::array<std::uint8_t, 32>& hash, std::size_t& size, ImageInfo& info) {
    File file = SD.open(imagePath_.data(), FILE_READ);
    return inspectFile(file, hash, size, info);
}

bool OeplMode::inspectFile(File& file, std::array<std::uint8_t, 32>& hash, std::size_t& size, ImageInfo& info) {
    if (!file || file.isDirectory() || !inspectJpeg(file, info)) {
        fail("Use baseline JPEG, max 512 KiB"); return false;
    }
    if (info.width != selected_.width || info.height != selected_.height) {
        fail("JPEG size must match AP tag size"); return false;
    }
    size = file.size();
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    bool valid = mbedtls_sha256_starts_ret(&ctx, 0) == 0;
    std::array<std::uint8_t, 1024> bytes{};
    std::size_t remaining = size;
    const auto start = millis();
    while (valid && remaining && !cancel_.load() && millis() - start < 15000) {
        const auto wanted = std::min(remaining, bytes.size());
        const auto n = file.read(bytes.data(), wanted);
        valid = n == wanted && mbedtls_sha256_update_ret(&ctx, bytes.data(), n) == 0;
        remaining -= n; delay(1);
    }
    valid = valid && !remaining && !cancel_.load() && mbedtls_sha256_finish_ret(&ctx, hash.data()) == 0;
    mbedtls_sha256_free(&ctx);
    if (!valid) fail(cancel_.load() ? "Cancelled" : "Image read/hash failed");
    return valid;
}

bool OeplMode::upload() {
    // Recheck registration, metadata and the reviewed file before sending any image.
    if (!fetchSelected()) return false;
    std::array<std::uint8_t, 32> hash{};
    std::size_t size = 0; ImageInfo info{};
    File file = SD.open(imagePath_.data(), FILE_READ);
    if (!inspectFile(file, hash, size, info)) return false;
    if (hash != imageHash_ || size != imageBytes_ || info.width != imageInfo_.width || info.height != imageInfo_.height) {
        fail("Image changed; select it again"); return false;
    }
    if (!file.seek(0)) { fail("Image cannot be read"); return false; }
    char boundary[48];
    std::snprintf(boundary, sizeof(boundary), "etag-%08lx%08lx%08lx",
                  static_cast<unsigned long>(esp_random()), static_cast<unsigned long>(esp_random()),
                  static_cast<unsigned long>(esp_random()));
    UploadBody body(file, multipartHead(selected_.mac.data(), dither_, boundary), multipartTail(boundary), progress_);
    const auto contentType = std::string("multipart/form-data; boundary=") + boundary;
    Socket wire(cancel_);
    const auto error = request(wire, endpoint_, "/imgupload", response_, &body, contentType.c_str(), 60000);
    if (error != Error::none) { fail(errorText(error)); return false; }
    if (!uploadAccepted(response_)) { fail("AP acceptance unclear; check AP"); return false; }
    fail("AP accepted image");
    return true;
}

bool OeplMode::work() {
    if (job_ == Job::connect) {
        WiFi.persistent(false);
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid_.data(), password_.data());
        const auto start = millis();
        while (!cancel_.load() && WiFi.status() != WL_CONNECTED && millis() - start < 20000) delay(50);
        if (cancel_.load() || WiFi.status() != WL_CONNECTED) {
            fail(cancel_.load() ? "Cancelled" : "Wi-Fi connection timed out"); return false;
        }
        offset_ = 0;
        return fetchTags();
    }
    if (job_ == Job::prepare) return inspectImage(imageHash_, imageBytes_, imageInfo_);
    if (WiFi.status() != WL_CONNECTED) { fail("Wi-Fi lost; reconnect from menu"); return false; }
    if (job_ == Job::list) return fetchTags();
    if (job_ == Job::details) return fetchSelected();
    return upload();
}

void OeplMode::loadImages() {
    images_.refresh(); jpegCount_ = 0; imagePage_ = 0;
    for (std::size_t i = 0; i < images_.size(); ++i) {
        std::string path = images_.path(i);
        for (char& c : path) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        const auto dot = path.rfind('.');
        if (dot != std::string::npos && (path.substr(dot) == ".jpg" || path.substr(dot) == ".jpeg"))
            jpegIndices_[jpegCount_++] = i;
    }
    view_ = View::images; draw();
}

void OeplMode::update() {
    if (waiting_) {
        if (!busy_.load(std::memory_order_acquire)) {
            waiting_ = false;
            if (leaveAfterWork_) { stop(); exit_ = true; return; }
            if (!okay_) view_ = View::result;
            else switch (job_) {
            case Job::connect: case Job::list: view_ = View::tags; break;
            case Job::details: view_ = View::details; break;
            case Job::prepare: view_ = View::review; break;
            case Job::upload: returnView_ = View::details; view_ = View::result; break;
            }
            draw();
            // A key pressed while the worker was finishing cannot authorize an
            // action on the newly presented view, especially Preview -> Send.
            return;
        } else {
            if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.keysState().backspace) {
                cancel_.store(true); leaveAfterWork_ = true;
            }
            if (millis() - lastDraw_ > 250) draw();
            return;
        }
    }
    if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) return;
    const auto& keys = M5Cardputer.Keyboard.keysState();
    if (keys.backspace) {
        switch (view_) {
        case View::setup: case View::tags: stop(); exit_ = true; return;
        case View::details: view_ = View::tags; break;
        case View::images: view_ = View::details; break;
        case View::review: view_ = View::images; break;
        case View::result: view_ = returnView_ == View::review ? View::details : returnView_; break;
        case View::working: break;
        }
        draw(); return;
    }
    switch (view_) {
    case View::setup:
        if (M5Cardputer.Keyboard.isKeyPressed('r')) { loadConfig(); draw(); }
        if (keys.enter && configured_) launch(Job::connect);
        break;
    case View::tags:
        for (unsigned row = 0; row < 3; ++row) if (M5Cardputer.Keyboard.isKeyPressed('1' + row)) {
            const auto index = listPage_ * 3 + row;
            if (index < tagCount_) { selected_ = tags_[index]; launch(Job::details); }
            return;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('n')) {
            if ((listPage_ + 1) * 3 < tagCount_) { ++listPage_; draw(); }
            else if (nextOffset_ >= 0) { offset_ = static_cast<unsigned>(nextOffset_); launch(Job::list); }
        } else if (M5Cardputer.Keyboard.isKeyPressed('p') && listPage_) { --listPage_; draw(); }
        else if (M5Cardputer.Keyboard.isKeyPressed('r')) { offset_ = 0; launch(Job::list); }
        break;
    case View::details:
        if (keys.enter && selected_.width && selected_.height) loadImages();
        else if (M5Cardputer.Keyboard.isKeyPressed('r')) launch(Job::details);
        break;
    case View::images:
        for (unsigned row = 0; row < 3; ++row) if (M5Cardputer.Keyboard.isKeyPressed('1' + row)) {
            const auto index = imagePage_ * 3 + row;
            if (index < jpegCount_) {
                std::snprintf(imagePath_.data(), imagePath_.size(), "%s", images_.path(jpegIndices_[index]));
                launch(Job::prepare);
            }
            return;
        }
        if (M5Cardputer.Keyboard.isKeyPressed('n') && (imagePage_ + 1) * 3 < jpegCount_) { ++imagePage_; draw(); }
        else if (M5Cardputer.Keyboard.isKeyPressed('p') && imagePage_) { --imagePage_; draw(); }
        else if (M5Cardputer.Keyboard.isKeyPressed('r')) loadImages();
        break;
    case View::review:
        if (M5Cardputer.Keyboard.isKeyPressed('d')) { dither_ = !dither_; draw(); }
        else if (keys.enter && previewReady_) launch(Job::upload);
        break;
    case View::result: case View::working: break;
    }
}

void OeplMode::line(const char* text, int y, std::uint32_t color) {
    canvas_.setTextColor(color, TFT_BLACK);
    char fitted[96]; std::snprintf(fitted, sizeof(fitted), "%s", text);
    std::size_t length = std::strlen(fitted);
    while (length && canvas_.textWidth(fitted) > 230) fitted[--length] = 0;
    canvas_.drawString(fitted, 5, y);
}

void OeplMode::draw() {
    lastDraw_ = millis();
    canvas_.fillScreen(TFT_BLACK); canvas_.setTextWrap(false);
    canvas_.setFont(&fonts::Font2); canvas_.setTextDatum(top_left);
    line("OpenEPaperLink / Wi-Fi", 2, TFT_CYAN);
    char text[96]{};
    switch (view_) {
    case View::setup:
        line(message_.data(), 30);
        line(configPath, 52, TFT_LIGHTGREY);
        if (configured_) {
            std::snprintf(text, sizeof(text), "AP %s:%u", endpoint_.host.data(), endpoint_.port);
            line(text, 77);
        }
        line("Enter Connect   R Reload", 101, TFT_GREEN);
        line("Bksp Exit", 118, TFT_LIGHTGREY);
        break;
    case View::tags:
        if (!tagCount_) line("No tags registered on this AP", 45);
        for (unsigned row = 0; row < 3; ++row) {
            const auto index = listPage_ * 3 + row;
            if (index >= tagCount_) break;
            const auto& tag = tags_[index];
            std::snprintf(text, sizeof(text), "%u %s", row + 1, tag.alias[0] ? tag.alias.data() : tag.mac.data());
            line(text, 31 + row * 23);
        }
        line("N/P Page  R Start  1-3 Select", 101, TFT_GREEN);
        line("Bksp Exit; Wi-Fi turns off", 118, TFT_LIGHTGREY);
        break;
    case View::details: {
        line(selected_.mac.data(), 25);
        std::snprintf(text, sizeof(text), "%u x %u  (AP reported)", selected_.width, selected_.height);
        line(text, 44);
        char battery[20] = "unknown", rssi[16] = "unknown", pending[16] = "unknown";
        if (selected_.batteryMv >= 0) std::snprintf(battery, sizeof(battery), "%d mV", selected_.batteryMv);
        if (selected_.rssi >= -127) std::snprintf(rssi, sizeof(rssi), "%d", selected_.rssi);
        if (selected_.pending >= 0) std::snprintf(pending, sizeof(pending), "%d", selected_.pending);
        std::snprintf(text, sizeof(text), "Battery %s  RSSI %s", battery, rssi);
        line(text, 63);
        std::snprintf(text, sizeof(text), "Pending: %s", pending);
        line(text, 82, TFT_LIGHTGREY);
        line("Enter SD images  R Status", 101, TFT_GREEN);
        line("Bksp Tags", 118, TFT_LIGHTGREY);
        break;
    }
    case View::images:
        if (!jpegCount_) { line("Put .jpg files in /etag/images", 40); line("Baseline JPEG; exact tag size", 65); }
        for (unsigned row = 0; row < 3; ++row) {
            const auto index = imagePage_ * 3 + row;
            if (index >= jpegCount_) break;
            std::snprintf(text, sizeof(text), "%u %s", row + 1, images_.name(jpegIndices_[index]));
            line(text, 31 + row * 23);
        }
        line("1-3 Preview  N/P Page  R Reload", 101, TFT_GREEN);
        line("Bksp Tag", 118, TFT_LIGHTGREY);
        break;
    case View::review: {
        line(selected_.mac.data(), 21, TFT_LIGHTGREY);
        const auto scale = std::min(225.0F / imageInfo_.width, 57.0F / imageInfo_.height);
        previewReady_ = canvas_.drawJpgFile(static_cast<fs::FS&>(SD), imagePath_.data(), 120, 68, 225, 57, 0, 0, scale, scale, middle_center);
        if (!previewReady_) line("JPEG preview failed", 63, TFT_RED);
        std::snprintf(text, sizeof(text), "Enter Send to AP  D Dither:%s", dither_ ? "on" : "off");
        line(text, 101, TFT_GREEN);
        line("Bksp Images", 118, TFT_LIGHTGREY);
        break;
    }
    case View::working:
        line(cancel_.load() ? "Stopping..." : job_ == Job::upload ? "Sending image to AP..." : "Working...", 37);
        if (job_ == Job::upload) {
            std::snprintf(text, sizeof(text), "%u%% read from SD", progress_.load()); line(text, 64);
        }
        line("Bksp Cancel and exit", 118, TFT_LIGHTGREY);
        break;
    case View::result:
        line(message_.data(), 35, okay_ ? TFT_GREEN : TFT_ORANGE);
        if (job_ == Job::upload) {
            line("Display update not verified.", 64);
            line("Check tag/AP before retrying.", 83, TFT_LIGHTGREY);
        }
        line("Bksp Back", 118, TFT_LIGHTGREY);
        break;
    }
    canvas_.pushSprite(0, 0);
}
} // namespace etag_host
#endif
