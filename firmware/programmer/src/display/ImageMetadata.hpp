// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace tagtinker::render::metadata {

struct ImageSize {
  std::uint32_t width = 0;
  std::uint32_t height = 0;
};

constexpr std::uint32_t maximumSourceDimension = 8192;
constexpr std::uint32_t maximumSourcePixels = 16U * 1024U * 1024U;

inline std::uint32_t big32(const std::uint8_t* bytes) {
  return (static_cast<std::uint32_t>(bytes[0]) << 24U) |
         (static_cast<std::uint32_t>(bytes[1]) << 16U) |
         (static_cast<std::uint32_t>(bytes[2]) << 8U) | bytes[3];
}

inline std::uint32_t little32(const std::uint8_t* bytes) {
  return static_cast<std::uint32_t>(bytes[0]) |
         (static_cast<std::uint32_t>(bytes[1]) << 8U) |
         (static_cast<std::uint32_t>(bytes[2]) << 16U) |
         (static_cast<std::uint32_t>(bytes[3]) << 24U);
}

inline std::uint16_t little16(const std::uint8_t* bytes) {
  return static_cast<std::uint16_t>(bytes[0]) |
         static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8U);
}

inline bool validDimensions(const std::uint32_t width, const std::uint32_t height) {
  return width > 0U && height > 0U && width <= maximumSourceDimension &&
         height <= maximumSourceDimension && height <= maximumSourcePixels / width;
}

inline bool pngSize(const std::uint8_t* header, const std::size_t length, ImageSize& size) {
  constexpr std::uint8_t signature[]{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  if (header == nullptr || length < 24U || std::memcmp(header, signature, sizeof(signature)) != 0 ||
      big32(header + 8U) != 13U || std::memcmp(header + 12U, "IHDR", 4U) != 0) {
    return false;
  }
  const std::uint32_t width = big32(header + 16U);
  const std::uint32_t height = big32(header + 20U);
  if (!validDimensions(width, height)) {
    return false;
  }
  size = {width, height};
  return true;
}

inline bool bmpSize(const std::uint8_t* header, const std::size_t length, ImageSize& size) {
  if (header == nullptr || length < 26U || header[0] != 'B' || header[1] != 'M') {
    return false;
  }
  const std::uint32_t rawWidth = little32(header + 18U);
  const std::uint32_t rawHeight = little32(header + 22U);
  const std::uint32_t dibSize = little32(header + 14U);
  if (dibSize == 12U) {
    const std::uint32_t width = little16(header + 18U);
    const std::uint32_t height = little16(header + 20U);
    if (!validDimensions(width, height)) {
      return false;
    }
    size = {width, height};
    return true;
  }
  if (dibSize < 40U) {
    return false;
  }
  if (rawWidth == 0U || rawWidth > static_cast<std::uint32_t>(std::numeric_limits<std::int32_t>::max()) ||
      rawHeight == 0U || rawHeight == 0x80000000U) {
    return false;
  }
  const auto signedHeight = static_cast<std::int32_t>(rawHeight);
  const std::uint32_t height = signedHeight < 0
                                   ? static_cast<std::uint32_t>(-static_cast<std::int64_t>(signedHeight))
                                   : static_cast<std::uint32_t>(signedHeight);
  if (!validDimensions(rawWidth, height)) {
    return false;
  }
  size = {rawWidth, height};
  return true;
}

inline bool qoiSize(const std::uint8_t* header, const std::size_t length, ImageSize& size) {
  if (header == nullptr || length < 14U || std::memcmp(header, "qoif", 4U) != 0 ||
      (header[12] != 3U && header[12] != 4U) || header[13] > 1U) {
    return false;
  }
  const std::uint32_t width = big32(header + 4U);
  const std::uint32_t height = big32(header + 8U);
  if (!validDimensions(width, height)) {
    return false;
  }
  size = {width, height};
  return true;
}

template <typename FileLike> std::uint16_t readBig16(FileLike& file) {
  const int high = file.read();
  const int low = file.read();
  return high < 0 || low < 0 ? 0U : static_cast<std::uint16_t>((high << 8U) | low);
}

template <typename FileLike> bool jpegSize(FileLike& file, ImageSize& size) {
  if (!file.seek(0U) || file.read() != 0xff || file.read() != 0xd8) {
    return false;
  }
  while (file.available()) {
    int prefix = file.read();
    while (prefix != 0xff && prefix >= 0) {
      prefix = file.read();
    }
    int marker = file.read();
    while (marker == 0xff) {
      marker = file.read();
    }
    if (marker < 0 || marker == 0xd9 || marker == 0xda) {
      return false;
    }
    if (marker == 0x01 || marker == 0xd8 || (marker >= 0xd0 && marker <= 0xd7)) {
      continue;
    }
    const std::uint16_t length = readBig16(file);
    if (length < 2U) {
      return false;
    }
    const bool startOfFrame =
        (marker >= 0xc0 && marker <= 0xc3) || (marker >= 0xc5 && marker <= 0xc7) ||
        (marker >= 0xc9 && marker <= 0xcb) || (marker >= 0xcd && marker <= 0xcf);
    if (startOfFrame) {
      if (length < 8U || file.read() < 0) {
        return false;
      }
      const std::uint32_t height = readBig16(file);
      const std::uint32_t width = readBig16(file);
      if (!validDimensions(width, height)) {
        return false;
      }
      size = {width, height};
      return true;
    }
    const std::size_t position = file.position();
    const std::size_t skip = static_cast<std::size_t>(length - 2U);
    if (position > std::numeric_limits<std::size_t>::max() - skip ||
        !file.seek(position + skip)) {
      return false;
    }
  }
  return false;
}

} // namespace tagtinker::render::metadata
