#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only

#include "ImageLibrary.hpp"

#include <SD.h>
#include "../sd_card.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>

namespace tagtinker {
namespace {

constexpr char imageDirectory[] = "/etag/images";

bool supported(const char* name) {
  const char* dot = std::strrchr(name, '.');
  if (dot == nullptr) {
    return false;
  }
  char extension[6]{};
  std::size_t length = 0;
  while (dot[length] != '\0' && length < sizeof(extension) - 1U) {
    extension[length] = static_cast<char>(std::tolower(static_cast<unsigned char>(dot[length])));
    ++length;
  }
  return std::strcmp(extension, ".png") == 0 || std::strcmp(extension, ".jpg") == 0 ||
         std::strcmp(extension, ".jpeg") == 0 || std::strcmp(extension, ".bmp") == 0 ||
         std::strcmp(extension, ".qoi") == 0;
}

} // namespace

ImageLibrary::Status ImageLibrary::refresh() {
  size_ = 0;
  for (auto& path : paths_) {
    path.fill('\0');
  }

  if (!etag_host::mountSd()) {
    status_ = Status::noCard;
    return status_;
  }
  if ((!SD.exists("/etag") && !SD.mkdir("/etag")) ||
      (!SD.exists(imageDirectory) && !SD.mkdir(imageDirectory))) {
    status_ = Status::error;
    return status_;
  }

  File directory = SD.open(imageDirectory);
  if (!directory || !directory.isDirectory()) {
    status_ = Status::error;
    return status_;
  }

  File entry = directory.openNextFile();
  while (entry && size_ < capacity) {
    const char* entryName = entry.name();
    if (!entry.isDirectory() && supported(entryName)) {
      if (entryName[0] == '/') {
        std::strncpy(paths_[size_].data(), entryName, pathCapacity - 1U);
      } else {
        std::snprintf(paths_[size_].data(), pathCapacity, "%s/%s", imageDirectory, entryName);
      }
      ++size_;
    }
    entry.close();
    entry = directory.openNextFile();
  }
  directory.close();

  std::sort(paths_.begin(), paths_.begin() + size_, [](const auto& left, const auto& right) {
    return std::strcmp(left.data(), right.data()) < 0;
  });
  status_ = Status::ready;
  return status_;
}

ImageLibrary::Status ImageLibrary::status() const { return status_; }

std::size_t ImageLibrary::size() const { return size_; }

const char* ImageLibrary::path(const std::size_t index) const {
  return index < size_ ? paths_[index].data() : nullptr;
}

const char* ImageLibrary::name(const std::size_t index) const {
  const char* fullPath = path(index);
  if (fullPath == nullptr) {
    return nullptr;
  }
  const char* slash = std::strrchr(fullPath, '/');
  return slash == nullptr ? fullPath : slash + 1;
}

} // namespace tagtinker

#endif // ETAG_CARDPUTER_ADV
