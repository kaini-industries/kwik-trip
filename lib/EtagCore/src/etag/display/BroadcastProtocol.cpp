// SPDX-License-Identifier: GPL-3.0-only

#include "BroadcastProtocol.hpp"

namespace tagtinker::broadcast {
namespace {

constexpr std::uint8_t graphicProtocol = 0x85;
constexpr std::uint8_t segmentProtocol = 0x84;

std::uint8_t graphicControl(const std::uint8_t page, const GraphicDuration duration) {
  std::uint8_t control = static_cast<std::uint8_t>(((page & 7U) << 3U) | 1U);
  if (duration == GraphicDuration::forever) {
    control |= 0x80U;
  }
  return control;
}

std::uint16_t graphicSeconds(const GraphicDuration duration) {
  switch (duration) {
  case GraphicDuration::seconds2:
    return 2;
  case GraphicDuration::seconds15:
    return 15;
  case GraphicDuration::minutes15:
    return 900;
  case GraphicDuration::forever:
    return 0;
  }
  return 0;
}

std::uint8_t segmentControl(const std::uint8_t page, const SegmentDuration duration) {
  if (duration == SegmentDuration::defaultPage) {
    return static_cast<std::uint8_t>(0x80U | ((page & 7U) << 3U));
  }
  return static_cast<std::uint8_t>(((page & 7U) << 3U) | static_cast<std::uint8_t>(duration));
}

} // namespace

Frame makeGraphicPageFrame(const std::uint8_t page, const GraphicDuration duration) {
  const std::uint16_t seconds = graphicSeconds(duration);
  const std::array<std::uint8_t, 6> payload{
      0x06, graphicControl(page, duration),           0,
      0,    static_cast<std::uint8_t>(seconds >> 8U), static_cast<std::uint8_t>(seconds & 0xffU)};
  return esl::makeFrame(graphicProtocol, {}, payload.data(), payload.size());
}

Frame makeSegmentPageFrame(const std::uint8_t page, const SegmentDuration duration) {
  const std::array<std::uint8_t, 4> payload{0xab, segmentControl(page, duration), 0, 0};
  return esl::makeFrame(segmentProtocol, {}, payload.data(), payload.size());
}

Frame makeDiagnosticsFrame() {
  constexpr std::array<std::uint8_t, 6> payload{0x06, 0xf1, 0, 0, 0, 10};
  return esl::makeFrame(graphicProtocol, {}, payload.data(), payload.size());
}

GraphicDuration next(const GraphicDuration duration) {
  switch (duration) {
  case GraphicDuration::seconds2:
    return GraphicDuration::seconds15;
  case GraphicDuration::seconds15:
    return GraphicDuration::minutes15;
  case GraphicDuration::minutes15:
    return GraphicDuration::forever;
  case GraphicDuration::forever:
    return GraphicDuration::seconds2;
  }
  return GraphicDuration::seconds15;
}

SegmentDuration next(const SegmentDuration duration) {
  switch (duration) {
  case SegmentDuration::seconds4:
    return SegmentDuration::seconds8;
  case SegmentDuration::seconds8:
    return SegmentDuration::seconds30;
  case SegmentDuration::seconds30:
    return SegmentDuration::minutes8;
  case SegmentDuration::minutes8:
    return SegmentDuration::minutes30;
  case SegmentDuration::minutes30:
    return SegmentDuration::hour1;
  case SegmentDuration::hour1:
    return SegmentDuration::minutes90;
  case SegmentDuration::minutes90:
    return SegmentDuration::defaultPage;
  case SegmentDuration::defaultPage:
    return SegmentDuration::seconds4;
  }
  return SegmentDuration::seconds8;
}

const char* durationLabel(const GraphicDuration duration) {
  switch (duration) {
  case GraphicDuration::seconds2:
    return "2 sec";
  case GraphicDuration::seconds15:
    return "15 sec";
  case GraphicDuration::minutes15:
    return "15 min";
  case GraphicDuration::forever:
    return "Forever";
  }
  return "15 sec";
}

const char* durationLabel(const SegmentDuration duration) {
  switch (duration) {
  case SegmentDuration::seconds4:
    return "4 sec";
  case SegmentDuration::seconds8:
    return "8 sec";
  case SegmentDuration::seconds30:
    return "30 sec";
  case SegmentDuration::minutes8:
    return "8 min";
  case SegmentDuration::minutes30:
    return "30 min";
  case SegmentDuration::hour1:
    return "1 hour";
  case SegmentDuration::minutes90:
    return "90 min";
  case SegmentDuration::defaultPage:
    return "Default";
  }
  return "8 sec";
}

const char* actionLabel(const Action action) {
  switch (action) {
  case Action::graphicPage:
    return "Graphic page";
  case Action::segmentPage:
    return "Segment page";
  case Action::diagnostics:
    return "Diagnostics";
  }
  return "Broadcast";
}

} // namespace tagtinker::broadcast
