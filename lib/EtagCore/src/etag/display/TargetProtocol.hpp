// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "EslFrame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tagtinker::target {

enum class Kind : std::uint8_t {
  unknown,
  graphic,
  segment,
};

enum class Color : std::uint8_t {
  mono,
  red,
  yellow,
  fourColor,
};

enum class ParseError : std::uint8_t {
  none,
  length,
  format,
  marker,
  range,
  checksum,
};

struct Profile {
  Kind kind = Kind::unknown;
  Color color = Color::mono;
  std::uint16_t width = 0;
  std::uint16_t height = 0;
  std::uint8_t imagePage = 0;
  bool pp16 = false;
  bool rotateClockwise = false;
};

struct Record {
  std::array<char, 25> name{};
  std::array<char, 18> barcode{};
  std::array<std::uint8_t, 4> wirePlid{};
  std::uint16_t typeCode = 0;
  Profile profile{};
  bool fromBarcode = false;
  bool profileOverridden = false;
};

struct ParseResult {
  Record record{};
  ParseError error = ParseError::none;

  [[nodiscard]] bool ok() const { return error == ParseError::none; }
};

[[nodiscard]] ParseResult parseBarcode(const char* barcode);
[[nodiscard]] bool detectProfile(std::uint16_t typeCode, Profile& profile);
[[nodiscard]] const char* profileName(std::uint16_t typeCode);
[[nodiscard]] const char* kindLabel(Kind kind);
[[nodiscard]] const char* colorLabel(Color color);
[[nodiscard]] const char* parseErrorLabel(ParseError error);
void formatPlid(const std::array<std::uint8_t, 4>& wirePlid, char destination[9]);

[[nodiscard]] esl::Frame makeWakeFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                       esl::Encoding encoding = esl::Encoding::pp16,
                                       bool requestAcknowledgement = false);
[[nodiscard]] esl::Frame makePingFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                       esl::Encoding encoding = esl::Encoding::pp16);
[[nodiscard]] esl::Frame makeBlinkFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                        std::uint16_t durationSeconds,
                                        esl::Encoding encoding = esl::Encoding::pp4);
[[nodiscard]] esl::Frame makeImageParametersFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                                  std::uint16_t byteCount, std::uint8_t compression,
                                                  std::uint8_t page, std::uint16_t width,
                                                  std::uint16_t height, esl::Encoding encoding);
[[nodiscard]] esl::Frame makeImageDataFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                            std::uint16_t index, const std::uint8_t data[20],
                                            esl::Encoding encoding);
[[nodiscard]] esl::Frame makeRefreshFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                          esl::Encoding encoding);

} // namespace tagtinker::target
