// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace tagtinker::esl {

enum class Encoding : std::uint8_t {
  pp4,
  pp16,
};

struct Frame {
  std::array<std::uint8_t, 40> bytes{};
  std::size_t size = 0;
  Encoding encoding = Encoding::pp4;
};

[[nodiscard]] Frame makeFrame(std::uint8_t protocol, const std::array<std::uint8_t, 4>& wirePlid,
                              const std::uint8_t* payload, std::size_t payloadSize,
                              Encoding encoding = Encoding::pp4);

} // namespace tagtinker::esl
