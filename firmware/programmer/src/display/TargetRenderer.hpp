// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <etag/display/ImageCodec.hpp>
#include <etag/display/TargetProtocol.hpp>

#include <FS.h>
#include <M5GFX.h>

namespace tagtinker::render {

enum class TextFont : std::uint8_t {
  sans,
  sansBold,
  serif,
  mono,
  rounded,
};

enum class TextSize : std::uint8_t {
  automatic,
  small,
  medium,
  large,
};

enum class TextAlign : std::uint8_t {
  left,
  center,
  right,
};

enum class TextVertical : std::uint8_t {
  top,
  middle,
  bottom,
};

enum class TextColor : std::uint8_t {
  black,
  white,
  red,
  yellow,
};

struct TextStyle {
  TextFont font = TextFont::sansBold;
  TextSize size = TextSize::automatic;
  TextAlign align = TextAlign::center;
  TextVertical vertical = TextVertical::middle;
  TextColor foreground = TextColor::black;
  TextColor background = TextColor::white;
};

[[nodiscard]] bool text(M5GFX& display, const target::Profile& profile, const char* value,
                        const TextStyle& style, image::Encoded& result);
[[nodiscard]] bool file(M5GFX& display, fs::FS& fileSystem, const char* path,
                        const target::Profile& profile, image::Encoded& result);

} // namespace tagtinker::render
