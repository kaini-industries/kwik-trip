#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only

#include "TargetStore.hpp"

#include <cstring>
#include <etag/display/Validation.hpp>

namespace tagtinker {
namespace {

constexpr std::uint32_t storeMagic = 0x54544156U; // TTAV
constexpr std::uint8_t storeVersion = 2;

struct StoredTarget {
  char name[25];
  char barcode[18];
  std::uint8_t wirePlid[4];
  std::uint16_t typeCode;
  std::uint16_t width;
  std::uint16_t height;
  std::uint8_t kind;
  std::uint8_t color;
  std::uint8_t imagePage;
  std::uint8_t flags;
};

struct StoredTargets {
  std::uint32_t magic;
  std::uint8_t version;
  std::uint8_t count;
  std::uint8_t reserved[2];
  StoredTarget targets[TargetStore::capacity];
};

StoredTarget pack(const target::Record& record) {
  StoredTarget stored{};
  std::memcpy(stored.name, record.name.data(), record.name.size());
  std::memcpy(stored.barcode, record.barcode.data(), record.barcode.size());
  std::memcpy(stored.wirePlid, record.wirePlid.data(), record.wirePlid.size());
  stored.typeCode = record.typeCode;
  stored.width = record.profile.width;
  stored.height = record.profile.height;
  stored.kind = static_cast<std::uint8_t>(record.profile.kind);
  stored.color = static_cast<std::uint8_t>(record.profile.color);
  stored.imagePage = record.profile.imagePage;
  stored.flags = static_cast<std::uint8_t>(
      (record.fromBarcode ? 1U : 0U) | (record.profile.pp16 ? 2U : 0U) |
      (record.profile.rotateClockwise ? 4U : 0U) | (record.profileOverridden ? 8U : 0U) |
      16U); // Bit 4 marks an explicit modulation choice.
  return stored;
}

bool unpack(const StoredTarget& stored, target::Record& record) {
  if (stored.kind > static_cast<std::uint8_t>(target::Kind::segment) ||
      stored.color > static_cast<std::uint8_t>(target::Color::fourColor) ||
      (stored.flags & 1U) == 0U ||
      (stored.wirePlid[0] == 0U && stored.wirePlid[1] == 0U && stored.wirePlid[2] == 0U &&
       stored.wirePlid[3] == 0U)) {
    return false;
  }
  record = {};
  std::memcpy(record.name.data(), stored.name, record.name.size());
  std::memcpy(record.barcode.data(), stored.barcode, record.barcode.size());
  std::memcpy(record.wirePlid.data(), stored.wirePlid, record.wirePlid.size());
  record.typeCode = stored.typeCode;
  record.profile.width = stored.width;
  record.profile.height = stored.height;
  record.profile.kind = static_cast<target::Kind>(stored.kind);
  record.profile.color = static_cast<target::Color>(stored.color);
  record.profile.imagePage = stored.imagePage;
  record.fromBarcode = (stored.flags & 1U) != 0U;
  record.profileOverridden = (stored.flags & 8U) != 0U;
  record.profile.rotateClockwise = (stored.flags & 4U) != 0U;
  target::Profile detected{};
  if (!record.profileOverridden && target::detectProfile(record.typeCode, detected)) {
    record.profile = detected;
  }
  record.profile.pp16 = record.profile.kind == target::Kind::graphic &&
                        ((stored.flags & 16U) == 0U || (stored.flags & 2U) != 0U);
  return target::validRecord(record);
}

} // namespace

bool TargetStore::begin() {
  size_ = 0;
  persistent_ = preferences_.begin("etag-ir", false);
  healthy_ = persistent_;
  if (!persistent_) return false;
  const auto length = preferences_.getBytesLength("targets");
  if (length == 0) return true;
  StoredTargets stored{};
  if (length != sizeof(stored) ||
      preferences_.getBytes("targets", &stored, sizeof(stored)) != sizeof(stored) ||
      stored.magic != storeMagic || stored.version != storeVersion || stored.count > capacity) {
    healthy_ = false;
    return false;
  }
  for (std::size_t index = 0; index < stored.count; ++index) {
    target::Record record{};
    if (!unpack(stored.targets[index], record)) { healthy_ = false; continue; }
    bool duplicate = false;
    for (std::size_t i = 0; i < size_; ++i) duplicate |= records_[i].wirePlid == record.wirePlid;
    if (duplicate) { healthy_ = false; continue; }
    records_[size_++] = record;
  }
  return healthy_;
}

std::size_t TargetStore::size() const { return size_; }

const target::Record* TargetStore::get(const std::size_t index) const {
  return index < size_ ? &records_[index] : nullptr;
}

int TargetStore::upsert(const target::Record& record) {
  if (!target::validRecord(record)) return -1;
  std::size_t index = 0;
  while (index < size_ && records_[index].wirePlid != record.wirePlid) ++index;
  if (index == capacity) return -1;
  const auto previous = records_[index];
  const auto oldSize = size_;
  records_[index] = record;
  if (index == size_) ++size_;
  healthy_ = save();
  if (!healthy_) { records_[index] = previous; size_ = oldSize; return -1; }
  return static_cast<int>(index);
}

bool TargetStore::erase(const std::size_t index) {
  if (index >= size_) return false;
  const auto previous = records_;
  const auto oldSize = size_;
  for (std::size_t cursor = index + 1; cursor < size_; ++cursor)
    records_[cursor - 1] = records_[cursor];
  records_[--size_] = {};
  healthy_ = save();
  if (!healthy_) { records_ = previous; size_ = oldSize; }
  return healthy_;
}

bool TargetStore::save() {
  if (!persistent_) {
    return false;
  }
  StoredTargets stored{};
  stored.magic = storeMagic;
  stored.version = storeVersion;
  stored.count = static_cast<std::uint8_t>(size_);
  for (std::size_t index = 0; index < size_; ++index) {
    stored.targets[index] = pack(records_[index]);
  }
  return preferences_.putBytes("targets", &stored, sizeof(stored)) == sizeof(stored);
}

} // namespace tagtinker

#endif // ETAG_CARDPUTER_ADV
