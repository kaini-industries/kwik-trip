// SPDX-License-Identifier: GPL-3.0-only
#include <etag/display/BoundedLine.hpp>
#include <etag/display/Validation.hpp>
#include <etag/display/JsonEnvelope.hpp>
#include "TargetStore.hpp"
#include <cassert>
#include <cstdio>
#include <cstring>

int main() {
  using namespace tagtinker;
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
