#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only

#include "TargetRenderer.hpp"
#include "ImageMetadata.hpp"
#include <esp_heap_caps.h>
#include <etag/display/Validation.hpp>

#include <array>
#include <cctype>
#include <cstring>
#include <utility>

namespace tagtinker::render {
namespace {

constexpr std::size_t maximumLines = 16;
using Line = std::array<char, 65>;

bool imageSize(fs::FS& fileSystem, const char* path, metadata::ImageSize& size) {
  File source = fileSystem.open(path, FILE_READ);
  if (!source) {
    return false;
  }
  std::array<std::uint8_t, 26> header{};
  const std::size_t read = source.read(header.data(), header.size());
  bool valid = false;
  if (read >= 8U && header[0] == 0x89 && header[1] == 'P' && header[2] == 'N' &&
      header[3] == 'G') {
    valid = metadata::pngSize(header.data(), read, size);
  } else if (read >= 2U && header[0] == 'B' && header[1] == 'M') {
    valid = metadata::bmpSize(header.data(), read, size);
  } else if (read >= 12U && std::memcmp(header.data(), "qoif", 4) == 0) {
    valid = metadata::qoiSize(header.data(), read, size);
  } else if (read >= 2U && header[0] == 0xff && header[1] == 0xd8) {
    valid = metadata::jpegSize(source, size);
  }
  source.close();
  return valid;
}

bool extract(M5Canvas& canvas, const target::Profile& profile, std::vector<std::uint8_t>& raw,
             std::size_t& writtenBits) {
  const std::size_t pixelCount = static_cast<std::size_t>(profile.width) * profile.height;
  const bool hasColorPlane = profile.color != target::Color::mono;
  raw.clear();
  if (!image::reserveBytes(raw, (pixelCount * (hasColorPlane ? 2U : 1U) + 7U) / 8U)) {
    return false;
  }
  writtenBits = 0;

  for (std::uint16_t y = 0; y < profile.height; ++y) {
    for (std::uint16_t x = 0; x < profile.width; ++x) {
      const std::uint16_t sourceX = profile.rotateClockwise ? y : x;
      const std::uint16_t sourceY =
          profile.rotateClockwise ? static_cast<std::uint16_t>(profile.width - 1U - x) : y;
      const std::uint32_t pixel = canvas.readPixelValue(sourceX, sourceY) & 3U;
      bool firstPlane = pixel != 0U;
      if (profile.color == target::Color::fourColor) {
        firstPlane = pixel == 1U || pixel == 2U;
      }
      if (!image::appendBit(raw, writtenBits, firstPlane)) {
        return false;
      }
    }
  }
  if (!hasColorPlane) {
    return true;
  }
  for (std::uint16_t y = 0; y < profile.height; ++y) {
    for (std::uint16_t x = 0; x < profile.width; ++x) {
      const std::uint16_t sourceX = profile.rotateClockwise ? y : x;
      const std::uint16_t sourceY =
          profile.rotateClockwise ? static_cast<std::uint16_t>(profile.width - 1U - x) : y;
      const std::uint32_t pixel = canvas.readPixelValue(sourceX, sourceY) & 3U;
      const bool secondPlane = profile.color == target::Color::fourColor ? pixel < 2U : pixel != 2U;
      if (!image::appendBit(raw, writtenBits, secondPlane)) {
        return false;
      }
    }
  }
  return true;
}

void setPalette(M5Canvas& canvas, const target::Color color) {
  canvas.setPaletteColor(0, TFT_BLACK);
  canvas.setPaletteColor(1, TFT_WHITE);
  canvas.setPaletteColor(2, color == target::Color::yellow ? TFT_YELLOW : TFT_RED);
  canvas.setPaletteColor(3, color == target::Color::fourColor ? TFT_YELLOW : TFT_WHITE);
}

bool drawFile(M5Canvas& canvas, fs::FS& fileSystem, const char* path, const std::uint16_t width,
              const std::uint16_t height, const float scale) {
  const char* dot = std::strrchr(path, '.');
  if (dot == nullptr) {
    return false;
  }
  char extension[6]{};
  std::size_t index = 0;
  while (dot[index] != '\0' && index < sizeof(extension) - 1U) {
    extension[index] = static_cast<char>(std::tolower(static_cast<unsigned char>(dot[index])));
    ++index;
  }
  const std::int32_t x = width / 2;
  const std::int32_t y = height / 2;
  if (std::strcmp(extension, ".png") == 0) {
    return canvas.drawPngFile(fileSystem, path, x, y, width, height, 0, 0, scale, scale,
                              middle_center);
  }
  if (std::strcmp(extension, ".jpg") == 0 || std::strcmp(extension, ".jpeg") == 0) {
    return canvas.drawJpgFile(fileSystem, path, x, y, width, height, 0, 0, scale, scale,
                              middle_center);
  }
  if (std::strcmp(extension, ".bmp") == 0) {
    return canvas.drawBmpFile(fileSystem, path, x, y, width, height, 0, 0, scale, scale,
                              middle_center);
  }
  if (std::strcmp(extension, ".qoi") == 0) {
    return canvas.drawQoiFile(fileSystem, path, x, y, width, height, 0, 0, scale, scale,
                              middle_center);
  }
  return false;
}

std::size_t wrap(M5Canvas& canvas, const char* value, const std::int32_t width,
                 std::array<Line, maximumLines>& lines) {
  std::size_t lineCount = 1;
  std::size_t index = 0;

  while (value[index] != '\0') {
    while (value[index] == ' ') {
      ++index;
    }
    if (value[index] == '\0') {
      break;
    }
    if (value[index] == '\n') {
      if (++lineCount > maximumLines) {
        return maximumLines + 1U;
      }
      ++index;
      continue;
    }

    std::array<char, 65> word{};
    std::size_t wordLength = 0;
    while (value[index] != '\0' && value[index] != ' ' && value[index] != '\n') {
      if (wordLength + 1U >= word.size()) {
        return maximumLines + 1U;
      }
      word[wordLength++] = value[index++];
    }

    Line& line = lines[lineCount - 1U];
    const std::size_t lineLength = std::strlen(line.data());
    const std::size_t separator = lineLength == 0U ? 0U : 1U;
    if (lineLength + separator + wordLength >= line.size()) {
      return maximumLines + 1U;
    }

    Line candidate = line;
    if (separator != 0U) {
      candidate[lineLength] = ' ';
    }
    std::memcpy(candidate.data() + lineLength + separator, word.data(), wordLength + 1U);
    if (canvas.textWidth(candidate.data()) <= width) {
      line = candidate;
      continue;
    }

    // Retry at a smaller font size instead of splitting a word.
    if (lineLength == 0U || canvas.textWidth(word.data()) > width || lineCount == maximumLines) {
      return maximumLines + 1U;
    }
    ++lineCount;
    lines[lineCount - 1U] = word;
  }
  return lineCount;
}

void setTextFont(M5Canvas& canvas, const TextFont font) {
  switch (font) {
  case TextFont::sans:
    canvas.setFont(&fonts::FreeSans9pt7b);
    break;
  case TextFont::sansBold:
    canvas.setFont(&fonts::FreeSansBold9pt7b);
    break;
  case TextFont::serif:
    canvas.setFont(&fonts::FreeSerif9pt7b);
    break;
  case TextFont::mono:
    canvas.setFont(&fonts::FreeMono9pt7b);
    break;
  case TextFont::rounded:
    canvas.setFont(&fonts::Font2);
    break;
  }
}

std::uint32_t textColor(const TextColor color, const target::Color palette) {
  switch (color) {
  case TextColor::white:
    return TFT_WHITE;
  case TextColor::red:
    return palette == target::Color::red || palette == target::Color::fourColor ? TFT_RED
                                                                                : TFT_BLACK;
  case TextColor::yellow:
    return palette == target::Color::yellow || palette == target::Color::fourColor ? TFT_YELLOW
                                                                                   : TFT_BLACK;
  case TextColor::black:
    return TFT_BLACK;
  }
  return TFT_BLACK;
}

std::int32_t fixedScale(M5Canvas& canvas, const std::uint16_t height, const TextSize size) {
  const std::int32_t baseHeight = canvas.fontHeight();
  std::int32_t targetHeight = static_cast<std::int32_t>(height) / 6;
  if (size == TextSize::small) {
    targetHeight = static_cast<std::int32_t>(height) / 10;
  } else if (size == TextSize::large) {
    targetHeight = static_cast<std::int32_t>(height) / 3;
  }
  const std::int32_t scale = baseHeight == 0 ? 1 : targetHeight / baseHeight;
  return scale < 1 ? 1 : (scale > 12 ? 12 : scale);
}

} // namespace

bool text(M5GFX& display, const target::Profile& profile, const char* value, const TextStyle& style,
          image::Encoded& result) {
  result = {};
  if (!target::validProfile(profile)) return false;
  // Account for vector growth and the temporary rendering strip on a no-PSRAM host.
  const std::size_t maxEncoded = std::min<std::size_t>(65536U,
      (std::size_t(profile.width) * profile.height / 8U) *
      (profile.color == target::Color::mono ? 1U : 2U) * 2U + 160U);
  if (ESP.getFreeHeap() < maxEncoded * 2U + 32768U ||
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < maxEncoded + 16384U) return false;
  if (profile.kind != target::Kind::graphic || profile.width == 0U || profile.height == 0U ||
      value == nullptr || value[0] == '\0') {
    return false;
  }
  const std::size_t pixelCount = static_cast<std::size_t>(profile.width) * profile.height;
  const bool hasColorPlane = profile.color != target::Color::mono;
  const std::size_t bitCount = pixelCount * (hasColorPlane ? 2U : 1U);
  if ((bitCount & 7U) != 0U) {
    return false;
  }

  const std::uint16_t canvasWidth = profile.rotateClockwise ? profile.height : profile.width;
  const std::uint16_t canvasHeight = profile.rotateClockwise ? profile.width : profile.height;

  // Font measurement does not require a full-size framebuffer.
  M5Canvas measureCanvas(&display);
  measureCanvas.setColorDepth(1);
  if (measureCanvas.createSprite(std::min<std::uint16_t>(canvasWidth, 320U), 16) == nullptr) {
    if (measureCanvas.createSprite(64, 16) == nullptr) {
      return false;
    }
  }
  setTextFont(measureCanvas, style.font);

  std::array<Line, maximumLines> lines{};
  std::size_t lineCount = maximumLines + 1U;
  bool fits = false;
  const std::int32_t maximumScale =
      style.size == TextSize::automatic ? 12 : fixedScale(measureCanvas, canvasHeight, style.size);
  const std::int32_t minimumScale = style.size == TextSize::automatic ? 1 : maximumScale;
  const std::int32_t marginX = 8;
  const std::int32_t marginY = 6;
  for (std::int32_t scale = maximumScale; scale >= minimumScale; --scale) {
    for (auto& line : lines) {
      line.fill('\0');
    }
    measureCanvas.setTextSize(scale);
    lineCount = wrap(measureCanvas, value, canvasWidth - marginX * 2, lines);
    if (lineCount <= maximumLines &&
        static_cast<std::int32_t>(lineCount) * measureCanvas.fontHeight() <=
            canvasHeight - marginY * 2) {
      fits = true;
      break;
    }
  }
  if (!fits) {
    measureCanvas.deleteSprite();
    return false;
  }

  const std::int32_t lineHeight = measureCanvas.fontHeight();
  const std::int32_t blockHeight = static_cast<std::int32_t>(lineCount) * lineHeight;
  std::int32_t top = marginY;
  if (style.vertical == TextVertical::middle) {
    top = (static_cast<std::int32_t>(canvasHeight) - blockHeight) / 2;
  } else if (style.vertical == TextVertical::bottom) {
    top = static_cast<std::int32_t>(canvasHeight) - blockHeight - marginY;
  }
  std::int32_t x = marginX;
  textdatum_t datum = middle_left;
  if (style.align == TextAlign::center) {
    x = canvasWidth / 2;
    datum = middle_center;
  } else if (style.align == TextAlign::right) {
    x = canvasWidth - marginX;
    datum = middle_right;
  }
  const std::int32_t chosenScale = measureCanvas.getTextSizeX();
  measureCanvas.deleteSprite();

  constexpr std::uint16_t stripRows = 16;
  M5Canvas strip(&display);
  strip.setColorDepth(profile.color == target::Color::mono ? 1 : 2);
  const auto stripWidth = profile.rotateClockwise ? stripRows : canvasWidth;
  const auto stripHeight = profile.rotateClockwise ? canvasHeight : stripRows;
  if (strip.createSprite(stripWidth, stripHeight) == nullptr)
    return false;
  setPalette(strip, profile.color);
  const auto foreground = textColor(style.foreground, profile.color);
  const auto background = textColor(style.background, profile.color);
  setTextFont(strip, style.font);
  strip.setTextSize(chosenScale);
  strip.setTextColor(foreground, background);
  strip.setTextDatum(datum);

  image::RleStreamEncoder encoder;
  if (!encoder.begin(bitCount)) {
    return false;
  }
  for (unsigned plane = 0; plane < (hasColorPlane ? 2U : 1U); ++plane) {
    for (std::uint16_t row = 0; row < profile.height; row += stripRows) {
      const auto rows = std::min<std::uint16_t>(stripRows, profile.height - row);
      strip.fillScreen(background);
      for (std::size_t line = 0; line < lineCount; ++line) {
        const auto centerY = top + static_cast<std::int32_t>(line) * lineHeight + lineHeight / 2;
        strip.drawString(lines[line].data(), x - (profile.rotateClockwise ? row : 0),
                         centerY - (profile.rotateClockwise ? 0 : row));
      }
      // Iterate in wire order even when the composition is rotated.
      for (std::uint16_t y = 0; y < rows; ++y) {
        for (std::uint16_t column = 0; column < profile.width; ++column) {
          const auto sourceX = profile.rotateClockwise ? y : column;
          const auto sourceY = profile.rotateClockwise ? profile.width - 1U - column : y;
          const auto pixel = strip.readPixelValue(sourceX, sourceY) & 3U;
          const bool value =
              plane == 0 ? (profile.color == target::Color::fourColor ? pixel == 1U || pixel == 2U
                                                                      : pixel != 0U)
                         : (profile.color == target::Color::fourColor ? pixel < 2U : pixel != 2U);
          if (!encoder.append(value))
            return false;
        }
      }
    }
  }
  return encoder.finish(result);
}

bool file(M5GFX& display, fs::FS& fileSystem, const char* path, const target::Profile& profile,
          image::Encoded& result) {
  result = {};
  if (path == nullptr || profile.kind != target::Kind::graphic || profile.width == 0U ||
      profile.height == 0U) {
    return false;
  }
  if (!target::validProfile(profile)) return false;
  const std::size_t rawBytes = std::size_t(profile.width) * profile.height *
      (profile.color == target::Color::mono ? 1U : 2U) / 8U;
  if (ESP.getFreeHeap() < rawBytes * 3U + 32768U ||
      heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < rawBytes + 16384U) return false;
  metadata::ImageSize sourceSize{};
  if (!imageSize(fileSystem, path, sourceSize)) {
    return false;
  }
  const std::uint16_t canvasWidth = profile.rotateClockwise ? profile.height : profile.width;
  const std::uint16_t canvasHeight = profile.rotateClockwise ? profile.width : profile.height;
  const float scaleX = static_cast<float>(canvasWidth) / static_cast<float>(sourceSize.width);
  const float scaleY = static_cast<float>(canvasHeight) / static_cast<float>(sourceSize.height);
  const float scale = scaleX < scaleY ? scaleX : scaleY;

  M5Canvas tagCanvas(&display);
  tagCanvas.setColorDepth(profile.color == target::Color::mono ? 1 : 2);
  if (tagCanvas.createSprite(canvasWidth, canvasHeight) == nullptr)
    return false;
  setPalette(tagCanvas, profile.color);
  tagCanvas.fillScreen(TFT_WHITE);
  if (!drawFile(tagCanvas, fileSystem, path, canvasWidth, canvasHeight, scale)) {
    tagCanvas.deleteSprite();
    return false;
  }

  std::vector<std::uint8_t> raw;
  std::size_t writtenBits = 0;
  const bool extracted = extract(tagCanvas, profile, raw, writtenBits);
  tagCanvas.deleteSprite();
  return extracted && image::encode(raw, writtenBits, result);
}

} // namespace tagtinker::render

#endif // ETAG_CARDPUTER_ADV
