// SPDX-License-Identifier: GPL-3.0-only
#include "ImageCodec.hpp"
#include "InfraredWaveform.hpp"
#include "TargetProtocol.hpp"
#include <cassert>
#include <iostream>
#include <random>

using namespace tagtinker;

static bool bit(const std::vector<std::uint8_t>& bytes, std::size_t index) {
  return (bytes.at(index / 8) >> (7 - index % 8)) & 1;
}

static void roundTrip(const std::vector<std::uint8_t>& raw, std::size_t count) {
  image::Encoded batch, streamed;
  assert(image::encode(raw, count, batch));
  image::RleStreamEncoder encoder;
  assert(encoder.begin(count));
  for (std::size_t i = 0; i < count; ++i)
    assert(encoder.append(bit(raw, i)));
  assert(!encoder.append(false));
  const bool streamFits = encoder.finish(streamed);
  for (const auto* encoded : {&batch, &streamed}) {
    if (encoded == &streamed && !streamFits)
      continue;
    assert(image::validate(encoded->bytes, encoded->compression, count));
    if (encoded->compression == 0) {
      for (std::size_t i = 0; i < count; ++i)
        assert(bit(encoded->bytes, i) == bit(raw, i));
      continue;
    }
    std::size_t cursor = 1, output = 0;
    bool color = bit(encoded->bytes, 0);
    while (output < count) {
      unsigned width = 1;
      while (!bit(encoded->bytes, cursor++))
        ++width;
      std::size_t length = 1;
      while (--width)
        length = length * 2 + bit(encoded->bytes, cursor++);
      assert(output + length <= count);
      while (length--)
        assert(bit(raw, output++) == color);
      color = !color;
    }
    assert(cursor == encoded->encodedBits);
  }
  if (batch.compression == 2) {
    assert(streamFits);
    assert(batch.bytes == streamed.bytes);
    assert(batch.encodedBits == streamed.encodedBits);
  }
}

int main() {
  assert(infrared::dataCopies(esl::Encoding::pp16, false) == 4);
  assert(infrared::dataCopies(esl::Encoding::pp16, true) == 3);
  assert(infrared::dataCopies(esl::Encoding::pp4, false) == 2);
  assert(infrared::dataCopies(esl::Encoding::pp4, true) == 2);
  for (unsigned length = 1; length <= 12; ++length) {
    for (unsigned value = 0; value < (1U << length); ++value) {
      std::vector<std::uint8_t> raw((length + 7) / 8);
      for (unsigned i = 0; i < length; ++i)
        if ((value >> i) & 1)
          raw[i / 8] |= 0x80 >> (i % 8);
      roundTrip(raw, length);
    }
  }
  std::mt19937 random(20260909);
  for (const unsigned type : {1339U, 1327U, 1336U}) {
    target::Profile profile;
    assert(target::detectProfile(type, profile));
    assert(profile.color == target::Color::red);
    const auto count = std::size_t(profile.width) * profile.height * 2;
    for (unsigned pattern = 0; pattern < 5; ++pattern) {
      std::vector<std::uint8_t> raw(count / 8);
      for (std::size_t i = 0; i < raw.size(); ++i) {
        raw[i] = pattern == 0   ? 0xff
                 : pattern == 1 ? 0
                 : pattern == 2 ? 0xaa
                 : pattern == 3 ? (i / 17 % 2 ? 0xff : 0)
                                : random();
      }
      // A one-pixel final run must still have a complete length code.
      if (pattern == 0)
        raw.back() = 0xfe;
      roundTrip(raw, count);
    }
  }
  image::RleStreamEncoder incomplete;
  assert(incomplete.begin(100));
  assert(incomplete.appendRun(99, true));
  image::Encoded result;
  assert(!incomplete.finish(result));
  assert(!incomplete.appendRun(2, true));

  image::RleStreamEncoder completed;
  assert(completed.begin(160));
  assert(completed.appendRun(160, true));
  assert(completed.finish(result));
  assert(!completed.finish(result));
  assert(!completed.append(false));
  assert(!completed.appendRun(0, false));

  image::RleStreamEncoder invalid;
  assert(!invalid.begin(0));
  assert(!invalid.begin(800U * 480U * 2U + 1U));
  assert(!image::validate(std::vector<std::uint8_t>(20), 2, 100));

  for (const auto encoding : {esl::Encoding::pp4, esl::Encoding::pp16}) {
    constexpr std::array<unsigned, 16> gaps{27, 51, 35, 43, 147, 123, 139, 131,
                                            83, 59, 75, 67, 91,  115, 99,  107};
    for (unsigned value = 0; value < 256; ++value) {
      esl::Frame frame;
      frame.encoding = encoding;
      frame.size = 1;
      frame.bytes[0] = value;
      infrared::Waveform wave;
      assert(infrared::encode(frame, 1000, wave));
      if (encoding == esl::Encoding::pp16) {
        assert(wave.size == 11);
        for (unsigned i = 0; i < 8; ++i) {
          assert(wave.pulses[i].gapTicks == (i == 7 ? 1470 : 270));
          assert(wave.pulses[i].burstTicks == 210);
        }
        assert(wave.pulses[8].gapTicks == gaps[value & 15] * 10);
        assert(wave.pulses[9].gapTicks == gaps[value >> 4] * 10);
      } else {
        assert(wave.size == 5);
        constexpr unsigned pp4[]{610, 2440, 1220, 1830};
        for (unsigned i = 0; i < 4; ++i) {
          assert(wave.pulses[i].burstTicks == 400);
          assert(wave.pulses[i].gapTicks == pp4[(value >> (i * 2)) & 3]);
        }
      }
      assert(wave.pulses[wave.size - 1].gapTicks == 10000);
    }
    esl::Frame largest;
    largest.size = largest.bytes.size();
    largest.encoding = encoding;
    infrared::Waveform wave;
    assert(infrared::encode(largest, 5000, wave));
    assert(wave.size < 192);
    assert(wave.pulses[wave.size - 2].gapTicks + wave.pulses[wave.size - 1].gapTicks == 50000);
    largest.size++;
    assert(!infrared::encode(largest, 1000, wave));
    const auto wake = target::makeWakeFrame({1, 2, 3, 4}, encoding);
    assert(infrared::encode(wake, 1000, wave));
    const auto repeats = infrared::wakeRepeats(wake, 1000);
    assert(repeats * wave.durationTicks >= infrared::wakeWindowUs * 10);
    assert((repeats - 1) * wave.durationTicks < infrared::wakeWindowUs * 10);
  }
  std::cout << "Codec round trips and waveform checks passed\n";
}
