// SPDX-License-Identifier: GPL-3.0-only

#include "ImageCodec.hpp"

#include <algorithm>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

namespace tagtinker::image {
namespace {

constexpr std::size_t maximumSourceBits = 800U * 480U * 2U;
constexpr std::size_t packetBits = 160U;
constexpr std::size_t packetBytes = packetBits / 8U;
constexpr std::size_t maximumEncodedBytes =
    (std::numeric_limits<std::uint16_t>::max() / packetBytes) * packetBytes;

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

bool paddedByteSize(const std::size_t encodedBits, std::size_t& bytes) {
  if (encodedBits == 0U || encodedBits > maximumEncodedBytes * 8U) {
    return false;
  }
  bytes = ((encodedBits + packetBits - 1U) / packetBits) * packetBytes;
  return bytes <= maximumEncodedBytes;
}

} // namespace

bool reserveBytes(std::vector<std::uint8_t>& bytes, const std::size_t capacity) noexcept {
#if defined(__cpp_exceptions)
  try {
    bytes.reserve(capacity);
  } catch (const std::bad_alloc&) {
    return false;
  } catch (const std::length_error&) {
    return false;
  }
#else
  bytes.reserve(capacity);
#endif
  return bytes.capacity() >= capacity;
}

bool appendBit(std::vector<std::uint8_t>& bytes, std::size_t& bitCount,
               const bool value) noexcept {
  if ((bitCount & 7U) == 0U) {
    if (bytes.size() == bytes.max_size()) {
      return false;
    }
#if defined(__cpp_exceptions)
    try {
      bytes.push_back(0);
    } catch (const std::bad_alloc&) {
      return false;
    } catch (const std::length_error&) {
      return false;
    }
#else
    bytes.push_back(0);
#endif
  }
  if (value) {
    bytes.back() |= static_cast<std::uint8_t>(0x80U >> (bitCount & 7U));
  }
  ++bitCount;
  return true;
}

bool encode(const std::vector<std::uint8_t>& raw, const std::size_t bitCount,
            Encoded& result) noexcept {
  result = {};
  if (bitCount == 0U || bitCount > maximumSourceBits ||
      raw.size() < (bitCount + 7U) / 8U) {
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
  const std::size_t encodedBits = useCompression ? compressedBits : bitCount;
  std::size_t paddedBytes = 0;
  if (!paddedByteSize(encodedBits, paddedBytes)) {
    return false;
  }

  // Allocate the final packet-aligned output exactly once. In particular, do not
  // reserve the unpadded RLE size and then double the live allocation on resize.
  std::vector<std::uint8_t> encoded;
  if (!reserveBytes(encoded, paddedBytes)) {
    return false;
  }
  if (useCompression) {
    std::size_t writtenBits = 0;
    runPixel = bitAt(raw, 0);
    if (!appendBit(encoded, writtenBits, runPixel)) {
      return false;
    }
    runLength = 1;
    for (std::size_t index = 1; index < bitCount; ++index) {
      const bool pixel = bitAt(raw, index);
      if (pixel == runPixel) {
        ++runLength;
        continue;
      }
      if (!appendRunCode(encoded, writtenBits, runLength)) {
        return false;
      }
      runPixel = pixel;
      runLength = 1;
    }
    if (!appendRunCode(encoded, writtenBits, runLength) || writtenBits != compressedBits) {
      return false;
    }
  } else {
    encoded.insert(encoded.end(), raw.begin(), raw.begin() + (bitCount + 7U) / 8U);
  }
  result.sourceBits = bitCount;
  result.encodedBits = encodedBits;
  result.compression = useCompression ? 2U : 0U;
  encoded.resize(paddedBytes, 0);
  result.bytes = std::move(encoded);
  return true;
}

bool validate(const std::vector<std::uint8_t>& bytes, const int compression,
              const std::size_t sourceBits) {
  if (sourceBits == 0 || sourceBits > maximumSourceBits || bytes.empty() ||
      bytes.size() > maximumEncodedBytes ||
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

bool RleStreamEncoder::begin(const std::size_t totalBits) noexcept {
  compressed_ = {};
  writtenBits_ = 0;
  totalPixelsSeen_ = 0;
  expectedBits_ = totalBits;
  runLength_ = 0;
  currentPixel_ = false;
  started_ = false;
  ready_ = false;
  finished_ = false;
  if (totalBits == 0U || totalBits > maximumSourceBits) {
    return false;
  }

  // One initial color bit plus a one-bit run code per source bit is the
  // largest possible RLE stream. Cap the allocation at the wire limit; later
  // appends fail before attempting to exceed it.
  const std::size_t maximumRleBits = totalBits + 1U;
  const std::size_t reserveSize = std::min(
      maximumEncodedBytes,
      ((maximumRleBits + packetBits - 1U) / packetBits) * packetBytes);
  ready_ = reserveBytes(compressed_, reserveSize);
  return ready_;
}

bool RleStreamEncoder::append(const bool pixel) {
  if (!ready_ || finished_ || totalPixelsSeen_ >= expectedBits_)
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
    const std::size_t codeBits = runCodeBits(runLength_);
    if (writtenBits_ > maximumEncodedBytes * 8U - codeBits ||
        !appendRunCode(compressed_, writtenBits_, runLength_)) {
      ready_ = false;
      return false;
    }
    currentPixel_ = pixel;
    runLength_ = 1;
  }
  ++totalPixelsSeen_;
  return true;
}

bool RleStreamEncoder::appendRun(const std::size_t count, const bool pixel) {
  if (!ready_ || finished_ || totalPixelsSeen_ > expectedBits_ ||
      count > expectedBits_ - totalPixelsSeen_)
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
    const std::size_t codeBits = runCodeBits(runLength_);
    if (writtenBits_ > maximumEncodedBytes * 8U - codeBits ||
        !appendRunCode(compressed_, writtenBits_, runLength_)) {
      ready_ = false;
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
  if (!ready_ || finished_ || !started_ || totalPixelsSeen_ == 0U ||
      totalPixelsSeen_ != expectedBits_) {
    return false;
  }

  const std::size_t codeBits = runCodeBits(runLength_);
  if (writtenBits_ > maximumEncodedBytes * 8U - codeBits ||
      !appendRunCode(compressed_, writtenBits_, runLength_)) {
    ready_ = false;
    return false;
  }

  result.sourceBits = totalPixelsSeen_;
  result.encodedBits = writtenBits_;
  result.compression = 2U; // RLE

  std::size_t paddedBytes = 0;
  if (!paddedByteSize(writtenBits_, paddedBytes) || paddedBytes > compressed_.capacity()) {
    result = {};
    ready_ = false;
    return false;
  }
  compressed_.resize(paddedBytes, 0);
  result.bytes = std::move(compressed_);
  finished_ = true;
  ready_ = false;
  return true;
}

} // namespace tagtinker::image
