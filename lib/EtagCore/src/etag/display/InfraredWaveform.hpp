// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "EslFrame.hpp"

namespace tagtinker::infrared {

constexpr std::uint32_t ticksPerMicrosecond = 10;
constexpr std::uint32_t wakeWindowUs = 4200000;

constexpr unsigned dataCopies(esl::Encoding encoding, bool fast) {
  return encoding == esl::Encoding::pp16 ? (fast ? 3U : 4U) : 2U;
}

struct Pulse {
  std::uint16_t burstTicks;
  std::uint16_t gapTicks;
};

struct Waveform {
  std::array<Pulse, 164> pulses{};
  std::size_t size = 0;
  std::uint32_t durationTicks = 0;
};

[[nodiscard]] bool encode(const esl::Frame& frame, std::uint32_t gapUs, Waveform& result);
[[nodiscard]] std::uint32_t wakeRepeats(const esl::Frame& frame, std::uint32_t gapUs);

} // namespace tagtinker::infrared
