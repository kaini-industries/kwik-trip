// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "EslFrame.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace tagtinker::broadcast {

enum class Action : std::uint8_t {
  graphicPage,
  segmentPage,
  diagnostics,
};

enum class GraphicDuration : std::uint8_t {
  seconds2,
  seconds15,
  minutes15,
  forever,
};

enum class SegmentDuration : std::uint8_t {
  seconds4,
  seconds8,
  seconds30,
  minutes8,
  minutes30,
  hour1,
  minutes90,
  defaultPage,
};

using esl::Encoding;
using esl::Frame;

[[nodiscard]] Frame makeGraphicPageFrame(std::uint8_t page, GraphicDuration duration);
[[nodiscard]] Frame makeSegmentPageFrame(std::uint8_t page, SegmentDuration duration);
[[nodiscard]] Frame makeDiagnosticsFrame();
[[nodiscard]] GraphicDuration next(GraphicDuration duration);
[[nodiscard]] SegmentDuration next(SegmentDuration duration);
[[nodiscard]] const char* durationLabel(GraphicDuration duration);
[[nodiscard]] const char* durationLabel(SegmentDuration duration);
[[nodiscard]] const char* actionLabel(Action action);

} // namespace tagtinker::broadcast
