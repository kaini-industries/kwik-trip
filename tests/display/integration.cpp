// SPDX-License-Identifier: GPL-3.0-only
#include <etag/display/BoundedLine.hpp>
#include <etag/display/Validation.hpp>
#include <etag/display/JsonEnvelope.hpp>
#include "ImageMetadata.hpp"
#include "KeyboardInput.hpp"
#include "TargetEditing.hpp"
#include "TargetStore.hpp"
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <utility>
#include <vector>

namespace {

void putBig32(std::uint8_t* bytes, const std::uint32_t value) {
  bytes[0] = static_cast<std::uint8_t>(value >> 24U);
  bytes[1] = static_cast<std::uint8_t>(value >> 16U);
  bytes[2] = static_cast<std::uint8_t>(value >> 8U);
  bytes[3] = static_cast<std::uint8_t>(value);
}

void putLittle32(std::uint8_t* bytes, const std::uint32_t value) {
  bytes[0] = static_cast<std::uint8_t>(value);
  bytes[1] = static_cast<std::uint8_t>(value >> 8U);
  bytes[2] = static_cast<std::uint8_t>(value >> 16U);
  bytes[3] = static_cast<std::uint8_t>(value >> 24U);
}

class MemoryFile {
public:
  explicit MemoryFile(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

  bool seek(const std::size_t position) {
    if (position > bytes_.size()) return false;
    position_ = position;
    return true;
  }
  [[nodiscard]] bool available() const { return position_ < bytes_.size(); }
  int read() { return available() ? bytes_[position_++] : -1; }
  [[nodiscard]] std::size_t position() const { return position_; }

private:
  std::vector<std::uint8_t> bytes_;
  std::size_t position_ = 0;
};

void imageMetadataChecks() {
  using tagtinker::render::metadata::ImageSize;
  ImageSize size{};

  std::array<std::uint8_t, 26> bmp{};
  bmp[0] = 'B';
  bmp[1] = 'M';
  putLittle32(bmp.data() + 14U, 40U);
  putLittle32(bmp.data() + 18U, 400U);
  putLittle32(bmp.data() + 22U, static_cast<std::uint32_t>(-300));
  assert(tagtinker::render::metadata::bmpSize(bmp.data(), bmp.size(), size));
  assert(size.width == 400U && size.height == 300U);
  putLittle32(bmp.data() + 22U, 0x80000000U);
  assert(!tagtinker::render::metadata::bmpSize(bmp.data(), bmp.size(), size));
  putLittle32(bmp.data() + 22U, 300U);
  putLittle32(bmp.data() + 18U, 0x80000001U);
  assert(!tagtinker::render::metadata::bmpSize(bmp.data(), bmp.size(), size));

  std::array<std::uint8_t, 24> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
  putBig32(png.data() + 8U, 13U);
  std::memcpy(png.data() + 12U, "IHDR", 4U);
  putBig32(png.data() + 16U, 800U);
  putBig32(png.data() + 20U, 480U);
  assert(tagtinker::render::metadata::pngSize(png.data(), png.size(), size));
  png[7] = 0;
  assert(!tagtinker::render::metadata::pngSize(png.data(), png.size(), size));

  std::array<std::uint8_t, 14> qoi{'q', 'o', 'i', 'f'};
  putBig32(qoi.data() + 4U, 296U);
  putBig32(qoi.data() + 8U, 128U);
  qoi[12] = 4U;
  qoi[13] = 0U;
  assert(tagtinker::render::metadata::qoiSize(qoi.data(), qoi.size(), size));
  qoi[12] = 2U;
  assert(!tagtinker::render::metadata::qoiSize(qoi.data(), qoi.size(), size));
  assert(!tagtinker::render::metadata::validDimensions(8192U, 8192U));

  MemoryFile jpeg({0xff, 0xd8, 0xff, 0xe0, 0x00, 0x04, 0x00, 0x00,
                   0xff, 0xc0, 0x00, 0x08, 0x08, 0x01, 0x2c, 0x01, 0x90, 0x00});
  assert(tagtinker::render::metadata::jpegSize(jpeg, size));
  assert(size.width == 400U && size.height == 300U);
  MemoryFile truncatedJpeg({0xff, 0xd8, 0xff, 0xe0, 0x00, 0x01});
  assert(!tagtinker::render::metadata::jpegSize(truncatedJpeg, size));
  MemoryFile hugeJpeg({0xff, 0xd8, 0xff, 0xc0, 0x00, 0x08, 0x08, 0x00, 0x10,
                       0x23, 0x29, 0x00});
  assert(!tagtinker::render::metadata::jpegSize(hugeJpeg, size));
}

} // namespace

int main() {
  using namespace tagtinker;
  Preferences::disk.clear();
  Preferences::unavailable = false;
  Preferences::failWrite = false;
  assert(flatJsonEnvelope("{\"cmd\":\"getState\"}"));
  assert(flatJsonEnvelope("{\"name\":\"braces {[]}\"}"));
  assert(!flatJsonEnvelope("{\"cmd\":{\"nested\":true}}"));
  assert(!flatJsonEnvelope("{\"cmd\":[1]}"));
  assert(!flatJsonEnvelope(std::string(512, '{') + std::string(512, '}')));
  assert(!flatJsonEnvelope("{\"cmd\":\"unterminated}"));
  auto r = target::parseBarcode("G4591371776312423").record;
  assert(target::validRecord(r));
  assert(!target::capabilities(r).displayUpdate);
  r.profile = {target::Kind::graphic, target::Color::red, 208, 112, 0, true, false};
  r.profileOverridden = true;
  r.profile.rotateClockwise = true;
  assert(target::capabilities(r).displayUpdate);
  assert(!target::capabilities(r).firmwareRead);
  assert(!target::capabilities(r).firmwareWrite);
  assert(!target::capabilities(r).acknowledged);
  for (auto width : {0, 7, 2049, 65535}) {
    auto bad = r; bad.profile.width = width;
    assert(!target::validRecord(bad));
  }
  auto bad = r; bad.profile.height = 2048; assert(!target::validRecord(bad));
  bad = r; bad.profile.imagePage = 8; assert(!target::validRecord(bad));
  bad = r; bad.wirePlid[0] ^= 1; assert(!target::validRecord(bad));
  bad = r; bad.name.back() = 'x'; assert(!target::validRecord(bad));
  bad = r; bad.profile.color = static_cast<target::Color>(255); assert(!target::validRecord(bad));

  BoundedLine<8> line;
  for (char c : std::string("123456789transmit")) line.push(c);
  assert(line.push('\n') == BoundedLine<8>::Event::rejected);
  for (char c : std::string("getState\r")) line.push(c);
  assert(line.push('\n') == BoundedLine<8>::Event::ready);
  assert(std::strcmp(line.line(), "getState") == 0);
  line.push('\0'); line.push('x');
  assert(line.push('\n') == BoundedLine<8>::Event::rejected);
  line.push('x'); line.reset(); line.push('y');
  assert(line.push('\n') == BoundedLine<8>::Event::ready);
  assert(std::strcmp(line.line(), "y") == 0);

  TargetStore store;
  assert(store.begin());
  assert(store.upsert(r) == 0);
  std::strcpy(r.name.data(), "Kitchen");
  assert(store.upsert(r) == 0 && store.size() == 1);
  TargetStore rebooted;
  assert(rebooted.begin() && rebooted.size() == 1);
  assert(std::strcmp(rebooted.get(0)->name.data(), "Kitchen") == 0);
  assert(rebooted.get(0)->profile.width == 208);

  const auto duplicate = target::parseBarcode(r.barcode.data());
  assert(duplicate.ok());
  const auto preserved = editing::preserveSavedRecord(store, duplicate.record);
  assert(std::strcmp(preserved.name.data(), "Kitchen") == 0);
  assert(preserved.profileOverridden && preserved.profile.width == 208U);
  assert(preserved.profile.rotateClockwise);
  assert(store.upsert(preserved) == 0);
  TargetStore duplicateReboot;
  assert(duplicateReboot.begin() && duplicateReboot.size() == 1U);
  assert(std::strcmp(duplicateReboot.get(0)->name.data(), "Kitchen") == 0);
  assert(duplicateReboot.get(0)->profileOverridden);
  assert(duplicateReboot.get(0)->profile.rotateClockwise);

  auto segment = preserved;
  editing::configureSegment(segment);
  assert(target::validRecord(segment));
  assert(segment.profile.width == 0U && segment.profile.height == 0U);
  assert(segment.profile.color == target::Color::mono && !segment.profile.pp16);
  assert(store.upsert(segment) == 0);
  TargetStore segmentReboot;
  assert(segmentReboot.begin() && segmentReboot.get(0)->profile.kind == target::Kind::segment);
  assert(segmentReboot.get(0)->profile.width == 0U && segmentReboot.get(0)->profile.height == 0U);

  auto graphic = segment;
  graphic.profile = {target::Kind::graphic, target::Color::red, 208, 112, 1, true, false};
  graphic.profileOverridden = true;
  assert(target::validRecord(graphic));
  assert(store.upsert(graphic) == 0);
  TargetStore graphicReboot;
  assert(graphicReboot.begin() && graphicReboot.get(0)->profile.kind == target::Kind::graphic);

  struct KeyState {
    bool backspace = false;
    bool del = false;
  } keys;
  keys.backspace = true;
  assert(keyboard::consoleEraseRequested(keys));
  keys.backspace = false;
  keys.del = true;
  assert(keyboard::consoleEraseRequested(keys));
  keys.del = false;
  assert(!keyboard::consoleEraseRequested(keys));
  imageMetadataChecks();

  assert(store.upsert(bad) == -1 && store.size() == 1);
  Preferences::failWrite = true;
  std::strcpy(r.name.data(), "Should not persist");
  assert(store.upsert(r) == -1 && !store.healthy());
  assert(std::strcmp(store.get(0)->name.data(), "Kitchen") == 0);
  assert(!store.erase(0) && store.size() == 1);
  Preferences::failWrite = false;
  assert(store.erase(0) && store.healthy());
  TargetStore empty;
  assert(empty.begin() && empty.size() == 0);
  Preferences::disk["targets"].resize(1); // Corrupt/truncated NVS blob.
  TargetStore corrupt;
  assert(!corrupt.begin() && corrupt.size() == 0);
  Preferences::unavailable = true;
  TargetStore unavailable;
  assert(!unavailable.begin() && unavailable.upsert(r) == -1);
  std::puts("Display capability, input bounds, and NVS recovery checks passed");
}
