// SPDX-License-Identifier: GPL-3.0-only

#include "InfraredWaveform.hpp"

namespace tagtinker::infrared {

bool encode(const esl::Frame& frame, const std::uint32_t gapUs, Waveform& result) {
  result = {};
  if (frame.size == 0 || frame.size > frame.bytes.size() || gapUs > 5000 ||
      (frame.encoding != esl::Encoding::pp4 && frame.encoding != esl::Encoding::pp16)) {
    return false;
  }
  constexpr std::array<std::uint16_t, 4> pp4Gaps{610, 2440, 1220, 1830};
  constexpr std::array<std::uint16_t, 16> pp16Gaps{270, 510, 350, 430, 1470, 1230, 1390, 1310,
                                                   830, 590, 750, 670, 910,  1150, 990,  1070};
  constexpr std::array<std::uint8_t, 4> prefix{0, 0, 0, 0x40};
  const bool pp16 = frame.encoding == esl::Encoding::pp16;
  const std::uint16_t burst = pp16 ? 210 : 400;
  const auto append = [&](const std::uint8_t byte) {
    for (unsigned shift = 0; shift < 8; shift += pp16 ? 4 : 2) {
      const auto gap = pp16 ? pp16Gaps[(byte >> shift) & 15] : pp4Gaps[(byte >> shift) & 3];
      result.pulses[result.size++] = {burst, gap};
      result.durationTicks += burst + gap;
    }
  };
  if (pp16) {
    for (const auto byte : prefix)
      append(byte);
  }
  for (std::size_t index = 0; index < frame.size; ++index)
    append(frame.bytes[index]);

  // Keep the quiet interval in RMT memory, including between different frames.
  const std::uint32_t gapTicks = gapUs == 0 ? 1 : gapUs * ticksPerMicrosecond;
  const auto firstGap = static_cast<std::uint16_t>(gapTicks > 32767 ? 32767 : gapTicks);
  result.pulses[result.size++] = {burst, firstGap};
  if (gapTicks > firstGap) {
    result.pulses[result.size++] = {0, static_cast<std::uint16_t>(gapTicks - firstGap)};
  }
  result.durationTicks += burst + gapTicks;
  return true;
}

std::uint32_t wakeRepeats(const esl::Frame& frame, const std::uint32_t gapUs) {
  Waveform waveform;
  if (!encode(frame, gapUs, waveform))
    return 0;
  const auto windowTicks = wakeWindowUs * ticksPerMicrosecond;
  return (windowTicks + waveform.durationTicks - 1) / waveform.durationTicks;
}

} // namespace tagtinker::infrared
