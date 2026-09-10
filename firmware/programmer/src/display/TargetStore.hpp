// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <etag/display/TargetProtocol.hpp>

#include <Preferences.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tagtinker {

class TargetStore final {
public:
  static constexpr std::size_t capacity = 9;

  bool begin();
  bool healthy() const { return healthy_; }
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] const target::Record* get(std::size_t index) const;
  [[nodiscard]] int upsert(const target::Record& record);
  bool erase(std::size_t index);

private:
  std::array<target::Record, capacity> records_{};
  std::size_t size_ = 0;
  Preferences preferences_{};
  bool persistent_ = false;
  bool healthy_ = false;

  bool save();
};

} // namespace tagtinker
