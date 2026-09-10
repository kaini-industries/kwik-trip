// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <cstddef>

namespace tagtinker {
// Reject an entire damaged/overlong line, including any command-shaped suffix.
template<std::size_t Capacity = 1024> class BoundedLine {
public:
  enum class Event { none, ready, rejected };
  Event push(char c) {
    if (c == '\n') {
      const bool bad = discarded_;
      data_[size_] = '\0';
      size_ = 0;
      discarded_ = false;
      return bad ? Event::rejected : Event::ready;
    }
    if (c == '\r') return Event::none;
    if (discarded_) return Event::none;
    if (static_cast<unsigned char>(c) < 32 || size_ == Capacity) {
      discarded_ = true;
      return Event::none;
    }
    data_[size_++] = c;
    return Event::none;
  }
  const char* line() const { return data_.data(); }
  void reset() { size_ = 0; discarded_ = false; data_[0] = '\0'; }
private:
  std::array<char, Capacity + 1> data_{};
  std::size_t size_ = 0;
  bool discarded_ = false;
};
} // namespace tagtinker
