// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "TargetProtocol.hpp"
#include <cstring>

namespace tagtinker::target {
inline bool validProfile(const Profile& p) {
  if (p.imagePage > 7 || p.color > Color::fourColor) return false;
  if (p.kind == Kind::segment) return p.width == 0 && p.height == 0 && !p.pp16;
  const auto pixels = std::uint32_t(p.width) * p.height;
  return p.kind == Kind::graphic && p.width >= 8 && p.height >= 8 &&
         p.width <= 2048 && p.height <= 2048 && pixels <= 384000 && pixels % 8 == 0;
}
inline bool validRecord(const Record& r) {
  if (!r.fromBarcode || r.barcode.back() != '\0' || r.name.back() != '\0' ||
      !validProfile(r.profile)) return false;
  const auto parsed = parseBarcode(r.barcode.data());
  return parsed.ok() && parsed.record.wirePlid == r.wirePlid &&
         parsed.record.typeCode == r.typeCode && r.wirePlid != std::array<std::uint8_t, 4>{};
}
struct Capabilities {
  bool displayUpdate;
  bool blink;
  bool firmwareRead = false;
  bool firmwareWrite = false;
  bool acknowledged = false;
};
inline Capabilities capabilities(const Record& r) {
  const bool graphic = validRecord(r) && r.profile.kind == Kind::graphic;
  return {graphic, graphic};
}
} // namespace tagtinker::target
