// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace tagtinker::image {

struct Encoded {
  std::vector<std::uint8_t> bytes{};
  std::size_t sourceBits = 0;
  std::size_t encodedBits = 0;
  std::uint8_t compression = 0;
};

[[nodiscard]] bool appendBit(std::vector<std::uint8_t>& bytes, std::size_t& bitCount, bool value);
[[nodiscard]] bool encode(const std::vector<std::uint8_t>& raw, std::size_t bitCount,
                          Encoded& result);

[[nodiscard]] bool validate(const std::vector<std::uint8_t>& bytes, int compression,
                            std::size_t sourceBits);

class RleStreamEncoder {
public:
  void begin(std::size_t totalBits);
  bool append(bool pixel);
  bool appendRun(std::size_t count, bool pixel);
  bool finish(Encoded& result);

private:
  std::vector<std::uint8_t> compressed_{};
  std::size_t writtenBits_ = 0;
  std::size_t totalPixelsSeen_ = 0;
  std::size_t expectedBits_ = 0;
  std::size_t runLength_ = 0;
  bool currentPixel_ = false;
  bool started_ = false;
};

} // namespace tagtinker::image
