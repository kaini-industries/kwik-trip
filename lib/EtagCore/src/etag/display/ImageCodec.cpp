// SPDX-License-Identifier: GPL-3.0-only

#include "ImageCodec.hpp"

#include <limits>
#include <utility>

namespace tagtinker::image {
namespace {

bool bitAt(const std::vector<std::uint8_t>& bytes, const std::size_t index) {
  return (bytes[index / 8U] & (0x80U >> (index & 7U))) != 0U;
}

bool appendRunCode(std::vector<std::uint8_t>& bytes, std::size_t& bitCount,
                   const std::size_t runLength) {
  std::size_t bits = 0;
  for (std::size_t value = runLength; value != 0U; value >>= 1U) {
    ++bits;
  }
  for (std::size_t zero = 1; zero < bits; ++zero) {
    if (!appendBit(bytes, bitCount, false)) {
      return false;
    }
  }
  for (std::size_t bit = bits; bit > 0U; --bit) {
    if (!appendBit(bytes, bitCount, (runLength & (std::size_t{1} << (bit - 1U))) != 0U)) {
      return false;
    }
  }
  return true;
}

std::size_t runCodeBits(const std::size_t runLength) {
  std::size_t width = 0;
  for (std::size_t value = runLength; value != 0U; value >>= 1U) {
    ++width;
  }
  return width * 2U - 1U;
}

} // namespace

bool appendBit(std::vector<std::uint8_t>& bytes, std::size_t& bitCount, const bool value) {
  if ((bitCount & 7U) == 0U) {
    if (bytes.size() == bytes.max_size()) {
      return false;
    }
    bytes.push_back(0);
  }
  if (value) {
    bytes.back() |= static_cast<std::uint8_t>(0x80U >> (bitCount & 7U));
  }
  ++bitCount;
  return true;
}

bool encode(const std::vector<std::uint8_t>& raw, const std::size_t bitCount, Encoded& result) {
  result = {};
  if (bitCount == 0U || bitCount > 800U * 480U * 2U || raw.size() < (bitCount + 7U) / 8U) {
    return false;
  }

  bool runPixel = bitAt(raw, 0);
  std::size_t runLength = 1;
  std::size_t compressedBits = 1;

  for (std::size_t index = 1; index < bitCount; ++index) {
    const bool pixel = bitAt(raw, index);
    if (pixel == runPixel) {
      ++runLength;
      continue;
    }
    compressedBits += runCodeBits(runLength);
    runPixel = pixel;
    runLength = 1;
  }
  compressedBits += runCodeBits(runLength);

  const bool useCompression = compressedBits < bitCount;
  if (useCompression) {
    std::vector<std::uint8_t> compressed;
    compressed.reserve((compressedBits + 7U) / 8U);
    std::size_t writtenBits = 0;
    runPixel = bitAt(raw, 0);
    if (!appendBit(compressed, writtenBits, runPixel)) {
      return false;
    }
    runLength = 1;
    for (std::size_t index = 1; index < bitCount; ++index) {
      const bool pixel = bitAt(raw, index);
      if (pixel == runPixel) {
        ++runLength;
        continue;
      }
      if (!appendRunCode(compressed, writtenBits, runLength)) {
        return false;
      }
      runPixel = pixel;
      runLength = 1;
    }
    if (!appendRunCode(compressed, writtenBits, runLength)) {
      return false;
    }
    result.bytes = std::move(compressed);
  } else {
    result.bytes = raw;
  }
  result.sourceBits = bitCount;
  result.encodedBits = useCompression ? compressedBits : bitCount;
  result.compression = useCompression ? 2U : 0U;

  // Pad to whole data packets.
  const std::size_t paddedBits = ((result.encodedBits + 159U) / 160U) * 160U;
  const std::size_t paddedBytes = paddedBits / 8U;
  if (paddedBytes > std::numeric_limits<std::uint16_t>::max()) {
    result = {};
    return false;
  }
  result.bytes.resize(paddedBytes, 0);
  return true;
}

bool validate(const std::vector<std::uint8_t>& bytes, const int compression,
              const std::size_t sourceBits) {
  if (sourceBits == 0 || sourceBits > 800U * 480U * 2U || bytes.empty() || bytes.size() > 65520U ||
      bytes.size() % 20U != 0)
    return false;
  if (compression == 0)
    return bytes.size() == ((sourceBits + 159U) / 160U) * 20U;
  if (compression != 2)
    return false;
  std::size_t position = 1;
  std::size_t decoded = 0;
  const auto available = bytes.size() * 8U;
  while (decoded < sourceBits) {
    std::size_t zeroes = 0;
    while (position < available && !bitAt(bytes, position)) {
      ++position;
      if (++zeroes > 19U)
        return false;
    }
    if (position + zeroes >= available)
      return false;
    std::size_t run = 0;
    for (std::size_t bit = 0; bit <= zeroes; ++bit) {
      run = (run << 1U) | (bitAt(bytes, position++) ? 1U : 0U);
    }
    if (run > sourceBits - decoded)
      return false;
    decoded += run;
  }
  return true;
}

void RleStreamEncoder::begin(const std::size_t totalBits) {
  compressed_.clear();
  compressed_.reserve(std::min<std::size_t>(totalBits / 8U + 160U, 8192U));
  writtenBits_ = 0;
  totalPixelsSeen_ = 0;
  expectedBits_ = totalBits;
  runLength_ = 0;
  currentPixel_ = false;
  started_ = false;
}

bool RleStreamEncoder::append(const bool pixel) {
  if (totalPixelsSeen_ >= expectedBits_ || compressed_.size() > 65520U)
    return false;
  if (!started_) {
    started_ = true;
    currentPixel_ = pixel;
    runLength_ = 1;
    totalPixelsSeen_ = 1;
    return appendBit(compressed_, writtenBits_, pixel);
  }

  if (pixel == currentPixel_) {
    ++runLength_;
  } else {
    if (!appendRunCode(compressed_, writtenBits_, runLength_)) {
      return false;
    }
    currentPixel_ = pixel;
    runLength_ = 1;
  }
  ++totalPixelsSeen_;
  return true;
}

bool RleStreamEncoder::appendRun(const std::size_t count, const bool pixel) {
  if (count > expectedBits_ - totalPixelsSeen_ || compressed_.size() > 65520U)
    return false;
  if (count == 0U) {
    return true;
  }
  if (!started_) {
    started_ = true;
    currentPixel_ = pixel;
    runLength_ = count;
    totalPixelsSeen_ = count;
    return appendBit(compressed_, writtenBits_, pixel);
  }

  if (pixel == currentPixel_) {
    runLength_ += count;
  } else {
    if (!appendRunCode(compressed_, writtenBits_, runLength_)) {
      return false;
    }
    currentPixel_ = pixel;
    runLength_ = count;
  }
  totalPixelsSeen_ += count;
  return true;
}

bool RleStreamEncoder::finish(Encoded& result) {
  result = {};
  if (!started_ || totalPixelsSeen_ == 0U || totalPixelsSeen_ != expectedBits_) {
    return false;
  }

  if (!appendRunCode(compressed_, writtenBits_, runLength_))
    return false;

  result.sourceBits = totalPixelsSeen_;
  result.encodedBits = writtenBits_;
  result.compression = 2U; // RLE

  const std::size_t paddedBits = ((writtenBits_ + 159U) / 160U) * 160U;
  const std::size_t paddedBytes = paddedBits / 8U;
  if (paddedBytes > std::numeric_limits<std::uint16_t>::max()) {
    result = {};
    return false;
  }
  compressed_.resize(paddedBytes, 0);
  result.bytes = std::move(compressed_);
  return true;
}

} // namespace tagtinker::image
