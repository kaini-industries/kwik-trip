// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <algorithm>
#include <cstring>
#include <map>
#include <string>
#include <vector>
class Preferences {
public:
  static inline std::map<std::string, std::vector<unsigned char>> disk;
  static inline bool unavailable = false;
  static inline bool failWrite = false;
  bool begin(const char*, bool) { return !unavailable; }
  std::size_t getBytesLength(const char* key) { return disk[key].size(); }
  std::size_t getBytes(const char* key, void* out, std::size_t size) {
    const auto n = std::min(size, disk[key].size());
    std::memcpy(out, disk[key].data(), n);
    return n;
  }
  std::size_t putBytes(const char* key, const void* data, std::size_t size) {
    if (failWrite) return 0;
    const auto* bytes = static_cast<const unsigned char*>(data);
    disk[key] = {bytes, bytes + size};
    return size;
  }
};
