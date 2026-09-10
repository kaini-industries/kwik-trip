// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <etag/display/EslFrame.hpp>

#include <driver/rmt.h>
#include "etag_catalog.h"

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>

namespace tagtinker {

class InfraredTransmitter final {
public:
  enum class Status : std::uint8_t {
    unavailable,
    idle,
    sending,
    succeeded,
    cancelled,
    error,
  };

  struct Step {
    esl::Frame frame{};
    std::uint16_t repeats = 1;
    std::uint16_t gapMs = 5;
  };

  struct Plan {
    std::array<Step, 3> steps{};
    std::size_t size = 0;
  };

  struct ImageTransfer {
    std::array<std::uint8_t, 4> wirePlid{};
    const std::uint8_t* data = nullptr;
    std::size_t dataSize = 0;
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t compression = 0;
    std::uint8_t page = 0;
    esl::Encoding encoding = esl::Encoding::pp16;
  };

  bool begin();
  bool start(const esl::Frame& frame);
  bool start(const Plan& plan);
  bool startImage(const ImageTransfer& transfer);
  void cancel();
  void setFast(bool fast) { fast_.store(fast); }
  [[nodiscard]] bool fast() const { return fast_.load(); }

  [[nodiscard]] Status status() const;
  [[nodiscard]] std::uint8_t progress() const;

private:
  static constexpr rmt_channel_t channel_ = RMT_CHANNEL_0;
  static constexpr gpio_num_t infraredPin_ = static_cast<gpio_num_t>(etag_build::kIrTx);
  static constexpr std::uint32_t carrierHz_ = 1250000;
  enum class Work : std::uint8_t {
    plan,
    image,
  };

  std::array<rmt_item32_t, 164> items_{};
  std::size_t itemCount_ = 0;
  Plan plan_{};
  ImageTransfer image_{};
  Work work_ = Work::plan;
  std::atomic<bool> fast_{false};
  bool imageFast_ = false;
  std::atomic<Status> status_{Status::unavailable};
  std::atomic<std::uint8_t> progress_{0};
  std::atomic<bool> cancelRequested_{false};

  bool claim();
  bool launch();
  bool encode(const esl::Frame& frame, std::uint32_t gapUs);

  static void transmitTask(void* context);
  void transmit();
  Status transmitPlan();
  Status transmitImage();
  bool send(const esl::Frame& frame, std::uint32_t repeats, std::uint32_t gapMicroseconds,
            std::uint32_t& completed, std::uint32_t total);
};

} // namespace tagtinker
