// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstdint>

namespace tagtinker::theme {

constexpr std::uint16_t rgb565(const std::uint8_t red, const std::uint8_t green,
                               const std::uint8_t blue) {
  return static_cast<std::uint16_t>(((red & 0xf8U) << 8U) | ((green & 0xfcU) << 3U) | (blue >> 3U));
}

constexpr std::uint16_t background = rgb565(0, 0, 0);
constexpr std::uint16_t text = rgb565(255, 255, 255);
constexpr std::uint16_t accent = rgb565(195, 169, 255);
constexpr std::uint16_t muted = rgb565(172, 179, 193);

} // namespace tagtinker::theme
