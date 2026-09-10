// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include <array>
#include <cstddef>

namespace tagtinker {

class ImageLibrary final {
public:
  static constexpr std::size_t capacity = 16;
  static constexpr std::size_t pathCapacity = 96;

  enum class Status {
    ready,
    noCard,
    error,
  };

  Status refresh();
  [[nodiscard]] Status status() const;
  [[nodiscard]] std::size_t size() const;
  [[nodiscard]] const char* path(std::size_t index) const;
  [[nodiscard]] const char* name(std::size_t index) const;

private:
  std::array<std::array<char, pathCapacity>, capacity> paths_{};
  std::size_t size_ = 0;
  Status status_ = Status::noCard;
};

} // namespace tagtinker
