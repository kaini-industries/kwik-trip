// SPDX-License-Identifier: GPL-3.0-only

#include "EslFrame.hpp"

namespace tagtinker::esl {
namespace {

std::uint16_t crc16(const std::uint8_t* data, const std::size_t size) {
  std::uint16_t crc = 0x8408;

  for (std::size_t index = 0; index < size; ++index) {
    crc ^= data[index];
    for (std::uint8_t bit = 0; bit < 8; ++bit) {
      crc = (crc & 1U) != 0U ? static_cast<std::uint16_t>((crc >> 1U) ^ 0x8408U)
                             : static_cast<std::uint16_t>(crc >> 1U);
    }
  }

  return crc;
}

} // namespace

Frame makeFrame(const std::uint8_t protocol, const std::array<std::uint8_t, 4>& wirePlid,
                const std::uint8_t* payload, const std::size_t payloadSize,
                const Encoding encoding) {
  Frame frame{};
  frame.encoding = encoding;

  const std::size_t bodySize = 1U + wirePlid.size() + payloadSize;
  if (payload == nullptr || bodySize + 2U > frame.bytes.size()) {
    return frame;
  }

  frame.bytes[frame.size++] = protocol;
  for (const auto byte : wirePlid) {
    frame.bytes[frame.size++] = byte;
  }
  for (std::size_t index = 0; index < payloadSize; ++index) {
    frame.bytes[frame.size++] = payload[index];
  }

  const std::uint16_t crc = crc16(frame.bytes.data(), frame.size);
  frame.bytes[frame.size++] = static_cast<std::uint8_t>(crc & 0xffU);
  frame.bytes[frame.size++] = static_cast<std::uint8_t>(crc >> 8U);
  return frame;
}

} // namespace tagtinker::esl
