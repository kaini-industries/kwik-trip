// SPDX-License-Identifier: GPL-3.0-only

#include "BroadcastProtocol.hpp"
#include "ImageCodec.hpp"
#include "TargetProtocol.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using tagtinker::broadcast::Encoding;
using tagtinker::broadcast::GraphicDuration;
using tagtinker::broadcast::SegmentDuration;

int main() {
  const auto segment = tagtinker::broadcast::makeSegmentPageFrame(1, SegmentDuration::seconds8);
  const std::array<std::uint8_t, 11> expectedSegment{0x84, 0, 0, 0,    0,   0xab,
                                                     0x09, 0, 0, 0xf2, 0xa7};
  assert(segment.size == expectedSegment.size());
  assert(segment.encoding == Encoding::pp4);
  for (std::size_t index = 0; index < expectedSegment.size(); ++index) {
    assert(segment.bytes[index] == expectedSegment[index]);
  }

  const auto diagnostics = tagtinker::broadcast::makeDiagnosticsFrame();
  const std::array<std::uint8_t, 13> expectedDiagnostics{0x85, 0, 0, 0,    0,    0x06, 0xf1,
                                                         0,    0, 0, 0x0a, 0x5d, 0x14};
  assert(diagnostics.size == expectedDiagnostics.size());
  assert(diagnostics.encoding == Encoding::pp4);
  for (std::size_t index = 0; index < expectedDiagnostics.size(); ++index) {
    assert(diagnostics.bytes[index] == expectedDiagnostics[index]);
  }

  const auto graphic = tagtinker::broadcast::makeGraphicPageFrame(7, GraphicDuration::minutes15);
  assert(graphic.size == 13);
  assert(graphic.encoding == Encoding::pp4);
  assert(graphic.bytes[6] == 0x39);
  assert(graphic.bytes[9] == 0x03);
  assert(graphic.bytes[10] == 0x84);

  const auto forever = tagtinker::broadcast::makeGraphicPageFrame(0, GraphicDuration::forever);
  assert(forever.bytes[6] == 0x81);
  assert(forever.bytes[9] == 0);
  assert(forever.bytes[10] == 0);

  const std::array<GraphicDuration, 4> graphicDurations{
      GraphicDuration::seconds2, GraphicDuration::seconds15, GraphicDuration::minutes15,
      GraphicDuration::forever};
  const std::array<std::uint8_t, 4> graphicFlags{1, 1, 1, 0x81};
  const std::array<std::uint16_t, 4> graphicSeconds{2, 15, 900, 0};

  const std::array<SegmentDuration, 8> segmentDurations{
      SegmentDuration::seconds4,  SegmentDuration::seconds8,   SegmentDuration::seconds30,
      SegmentDuration::minutes8,  SegmentDuration::minutes30,  SegmentDuration::hour1,
      SegmentDuration::minutes90, SegmentDuration::defaultPage};
  const std::array<std::uint8_t, 8> segmentFlags{0, 1, 2, 3, 4, 5, 6, 0x80};

  for (std::uint8_t page = 0; page < 8; ++page) {
    for (std::size_t duration = 0; duration < graphicDurations.size(); ++duration) {
      const auto frame =
          tagtinker::broadcast::makeGraphicPageFrame(page, graphicDurations[duration]);
      for (std::size_t plidByte = 1; plidByte <= 4; ++plidByte) {
        assert(frame.bytes[plidByte] == 0);
      }
      assert(frame.bytes[6] == static_cast<std::uint8_t>((page << 3U) | graphicFlags[duration]));
      assert(frame.bytes[9] == graphicSeconds[duration] >> 8U);
      assert(frame.bytes[10] == (graphicSeconds[duration] & 0xffU));
    }

    for (std::size_t duration = 0; duration < segmentDurations.size(); ++duration) {
      const auto frame =
          tagtinker::broadcast::makeSegmentPageFrame(page, segmentDurations[duration]);
      for (std::size_t plidByte = 1; plidByte <= 4; ++plidByte) {
        assert(frame.bytes[plidByte] == 0);
      }
      assert(frame.bytes[6] == static_cast<std::uint8_t>((page << 3U) | segmentFlags[duration]));
    }
  }

  const auto target = tagtinker::target::parseBarcode("G4591371776312423");
  assert(target.ok());
  assert(target.record.wirePlid[0] == 0x63);
  assert(target.record.wirePlid[1] == 0x45);
  assert(target.record.wirePlid[2] == 0x01);
  assert(target.record.wirePlid[3] == 0xe7);
  assert(target.record.typeCode == 1242);
  assert(target.record.profile.kind == tagtinker::target::Kind::segment);

  tagtinker::target::Profile colorProfile{};
  assert(tagtinker::target::detectProfile(1626, colorProfile));
  assert(colorProfile.kind == tagtinker::target::Kind::graphic);
  assert(colorProfile.color == tagtinker::target::Color::fourColor);
  assert(colorProfile.width == 296);
  assert(colorProfile.height == 152);
  assert(colorProfile.imagePage == 1);
  assert(colorProfile.pp16);
  assert(!colorProfile.rotateClockwise);
  tagtinker::target::Profile unknownProfile{};
  assert(!tagtinker::target::detectProfile(9999, unknownProfile));

  char formattedPlid[9]{};
  tagtinker::target::formatPlid(target.record.wirePlid, formattedPlid);
  assert(std::string(formattedPlid) == "E7014563");

  assert(tagtinker::target::parseBarcode("G4591371776312424").error ==
         tagtinker::target::ParseError::checksum);
  assert(tagtinker::target::parseBarcode("G4641371776312429").error ==
         tagtinker::target::ParseError::range);
  assert(tagtinker::target::parseBarcode("G4591541776312422").error ==
         tagtinker::target::ParseError::range);
  assert(tagtinker::target::parseBarcode("G4591376553512423").error ==
         tagtinker::target::ParseError::range);

  const auto wake = tagtinker::target::makeWakeFrame(target.record.wirePlid, Encoding::pp4);
  const std::array<std::uint8_t, 34> expectedWake{
      0x85, 0x63, 0x45, 0x01, 0xe7, 0x17, 0x01, 0x00, 0x00, 0x00, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x8e, 0x61};
  assert(wake.size == expectedWake.size());
  for (std::size_t index = 0; index < expectedWake.size(); ++index) {
    assert(wake.bytes[index] == expectedWake[index]);
  }

  const auto ping = tagtinker::target::makePingFrame(target.record.wirePlid, Encoding::pp16);
  assert(ping.size == 32);
  assert(ping.bytes[5] == 0x97);
  assert(ping.bytes[30] == 0xff);
  assert(ping.bytes[31] == 0x87);

  const auto acknowledgedWake =
      tagtinker::target::makeWakeFrame(target.record.wirePlid, Encoding::pp16, true);
  assert(acknowledgedWake.size == 32);
  assert(acknowledgedWake.bytes[5] == 0x97);
  assert(acknowledgedWake.bytes[30] == 0xff);
  assert(acknowledgedWake.bytes[31] == 0x87);

  const auto pp16Wake = tagtinker::target::makeWakeFrame(target.record.wirePlid, Encoding::pp16);
  assert(pp16Wake.encoding == Encoding::pp16);
  assert(pp16Wake.size == expectedWake.size());
  for (std::size_t index = 0; index < expectedWake.size(); ++index) {
    assert(pp16Wake.bytes[index] == expectedWake[index]);
  }

  const auto blink = tagtinker::target::makeBlinkFrame(target.record.wirePlid, 5);
  const std::array<std::uint8_t, 13> expectedBlink{0x85, 0x63, 0x45, 0x01, 0xe7, 0x06, 0x49,
                                                   0x00, 0x00, 0x00, 0x05, 0xed, 0x91};
  assert(blink.size == expectedBlink.size());
  for (std::size_t index = 0; index < expectedBlink.size(); ++index) {
    assert(blink.bytes[index] == expectedBlink[index]);
  }
  for (const std::uint16_t duration : {1U, 10U, 30U, 60U, 120U}) {
    const auto timedBlink = tagtinker::target::makeBlinkFrame(target.record.wirePlid, duration);
    assert(timedBlink.bytes[6] == 0x49);
    assert(timedBlink.bytes[9] == static_cast<std::uint8_t>(duration >> 8U));
    assert(timedBlink.bytes[10] == static_cast<std::uint8_t>(duration & 0xffU));
  }

  const auto parameters = tagtinker::target::makeImageParametersFrame(target.record.wirePlid, 40, 2,
                                                                      0, 172, 72, Encoding::pp16);
  const std::array<std::uint8_t, 34> expectedParameters{
      0x85, 0x63, 0x45, 0x01, 0xe7, 0x34, 0x00, 0x00, 0x00, 0x05, 0x00, 0x28,
      0x00, 0x02, 0x00, 0x00, 0xac, 0x00, 0x48, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x88, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xae, 0x37};
  assert(parameters.size == expectedParameters.size());
  assert(parameters.encoding == Encoding::pp16);
  for (std::size_t index = 0; index < expectedParameters.size(); ++index) {
    assert(parameters.bytes[index] == expectedParameters[index]);
  }

  std::array<std::uint8_t, 20> frameData{};
  for (std::size_t index = 0; index < frameData.size(); ++index) {
    frameData[index] = static_cast<std::uint8_t>(index);
  }
  const auto dataFrame = tagtinker::target::makeImageDataFrame(target.record.wirePlid, 3,
                                                               frameData.data(), Encoding::pp16);
  assert(dataFrame.size == 34);
  assert(dataFrame.bytes[9] == 0x20);
  assert(dataFrame.bytes[10] == 0x00);
  assert(dataFrame.bytes[11] == 0x03);
  assert(dataFrame.bytes[32] == 0x2a);
  assert(dataFrame.bytes[33] == 0x62);

  const auto refresh = tagtinker::target::makeRefreshFrame(target.record.wirePlid, Encoding::pp16);
  assert(refresh.size == 30);
  assert(refresh.bytes[9] == 0x01);
  assert(refresh.bytes[28] == 0x08);
  assert(refresh.bytes[29] == 0x8f);

  std::vector<std::uint8_t> flat(20, 0);
  tagtinker::image::Encoded compressed{};
  assert(tagtinker::image::encode(flat, 160, compressed));
  assert(compressed.compression == 2);
  assert(compressed.encodedBits == 16);
  assert(compressed.bytes.size() == 20);
  assert(compressed.bytes[0] == 0x00);
  assert(compressed.bytes[1] == 0xa0);

  std::vector<std::uint8_t> alternating(20, 0xaa);
  tagtinker::image::Encoded raw{};
  assert(tagtinker::image::encode(alternating, 160, raw));
  assert(raw.compression == 0);
  assert(raw.encodedBits == 160);
  assert(raw.bytes.size() == 20);
  for (std::size_t index = 0; index < alternating.size(); ++index) {
    assert(raw.bytes[index] == alternating[index]);
  }

  std::vector<std::uint8_t> documentedRun;
  std::size_t documentedBits = 0;
  for (std::size_t index = 0; index < 13; ++index) {
    assert(tagtinker::image::appendBit(documentedRun, documentedBits, true));
  }
  for (std::size_t index = 0; index < 59; ++index) {
    assert(tagtinker::image::appendBit(documentedRun, documentedBits, false));
  }
  tagtinker::image::Encoded documented{};
  assert(tagtinker::image::encode(documentedRun, documentedBits, documented));
  assert(documented.compression == 2);
  assert(documented.encodedBits == 19);
  assert(documented.bytes[0] == 0x8d);
  assert(documented.bytes[1] == 0x07);
  assert(documented.bytes[2] == 0x60);
}
