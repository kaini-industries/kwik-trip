// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#ifdef ETAG_CARDPUTER_ADV
#include <etag/oepl/Api.hpp>
#include <FS.h>
#include <M5GFX.h>
#include <array>
#include <atomic>
#include "../display/ImageLibrary.hpp"

namespace etag_host {
class OeplMode final {
public:
    explicit OeplMode(M5Canvas& canvas) : canvas_(canvas) {}
    void begin();
    void update();
    void stop(); // Only after the worker has stopped.
    bool exitRequested() const { return exit_; }

private:
    struct Tag {
        std::array<char, 17> mac{};
        std::array<char, 25> alias{};
        int hwType = -1, batteryMv = -1, rssi = -129, pending = -1;
        std::uint16_t width = 0, height = 0;
    };
    enum class View { setup, tags, details, images, review, working, result };
    enum class Job { connect, list, details, prepare, upload };
    M5Canvas& canvas_;
    View view_ = View::setup, returnView_ = View::setup;
    Job job_ = Job::connect;
    std::atomic<bool> busy_{false}, cancel_{false};
    std::atomic<unsigned> progress_{0};
    bool waiting_ = false, leaveAfterWork_ = false, exit_ = false, okay_ = false;
    bool configured_ = false, previewReady_ = false, dither_ = false;
    std::uint32_t lastDraw_ = 0;
    std::array<char, 33> ssid_{};
    std::array<char, 65> password_{};
    etag::oepl::Endpoint endpoint_{};
    std::array<char, 96> message_{};
    std::array<Tag, 32> tags_{};
    std::size_t tagCount_ = 0, listPage_ = 0, imagePage_ = 0;
    unsigned offset_ = 0;
    int nextOffset_ = -1;
    Tag selected_{};
    tagtinker::ImageLibrary images_;
    std::array<std::size_t, 16> jpegIndices_{};
    std::size_t jpegCount_ = 0;
    std::array<char, 96> imagePath_{};
    std::array<std::uint8_t, 32> imageHash_{};
    std::size_t imageBytes_ = 0;
    etag::oepl::ImageInfo imageInfo_{};
    etag::oepl::Response response_{};

    void loadConfig();
    void launch(Job job);
    static void task(void* argument);
    bool work();
    bool fetch(const char* path);
    bool fetchTags();
    bool fetchSelected();
    bool upload();
    bool inspectImage(std::array<std::uint8_t, 32>& hash, std::size_t& size,
                      etag::oepl::ImageInfo& info);
    bool inspectFile(File& file, std::array<std::uint8_t, 32>& hash, std::size_t& size,
                     etag::oepl::ImageInfo& info);
    void loadImages();
    void draw();
    void line(const char* text, int y, std::uint32_t color = TFT_WHITE);
    void fail(const char* message);
};
} // namespace etag_host
#endif
