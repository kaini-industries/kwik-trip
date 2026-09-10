// SPDX-License-Identifier: GPL-3.0-only

#include "TargetProtocol.hpp"

#include <cstdio>
#include <cstring>

namespace tagtinker::target {
namespace {

struct KnownProfile {
  std::uint16_t typeCode;
  std::uint16_t width;
  std::uint16_t height;
  Kind kind;
  Color color;
  const char* name;
  std::uint8_t imagePage;
};

// Unknown barcode types require a manual display profile.
constexpr std::array<KnownProfile, 45> knownProfiles{{
    {1206, 0, 0, Kind::segment, Color::mono, "Continuum E2 HCS", 0},
    {1207, 0, 0, Kind::segment, Color::mono, "Continuum E2 HCN", 0},
    {1217, 0, 0, Kind::segment, Color::mono, "Continuum E5 HCS", 0},
    {1219, 0, 0, Kind::segment, Color::mono, "Continuum E5 HCN", 0},
    {1240, 0, 0, Kind::segment, Color::mono, "Continuum E4 HCS", 0},
    {1241, 0, 0, Kind::segment, Color::mono, "Continuum E4 HCN", 0},
    {1242, 0, 0, Kind::segment, Color::mono, "Continuum E4 HCN FZ", 0},
    {1243, 0, 0, Kind::segment, Color::mono, "Continuum E4 HCW", 0},
    {1265, 0, 0, Kind::segment, Color::mono, "Continuum E5 HCS", 0},
    {1275, 320, 192, Kind::graphic, Color::mono, "DM110", 0},
    {1276, 320, 140, Kind::graphic, Color::mono, "DM90", 0},
    {1291, 0, 0, Kind::segment, Color::mono, "FVL Promoline 3-16", 0},
    {1300, 172, 72, Kind::graphic, Color::mono, "DM3370", 0},
    {1314, 400, 300, Kind::graphic, Color::mono, "SmartTag HD110", 0},
    {1315, 296, 128, Kind::graphic, Color::mono, "SmartTag HD L", 0},
    {1317, 152, 152, Kind::graphic, Color::mono, "SmartTag HD S", 0},
    {1318, 208, 112, Kind::graphic, Color::mono, "SmartTag HD M", 0},
    {1319, 800, 480, Kind::graphic, Color::mono, "SmartTag HD200", 0},
    {1322, 152, 152, Kind::graphic, Color::mono, "SmartTag HD S", 0},
    {1324, 208, 112, Kind::graphic, Color::mono, "SmartTag HD M FZ", 0},
    {1327, 208, 112, Kind::graphic, Color::red, "SmartTag HD M Red", 0},
    {1328, 296, 128, Kind::graphic, Color::red, "SmartTag HD L Red", 0},
    {1336, 400, 300, Kind::graphic, Color::red, "SmartTag HD110 Red", 0},
    {1339, 152, 152, Kind::graphic, Color::red, "SmartTag HD S Red", 0},
    {1340, 800, 480, Kind::graphic, Color::red, "SmartTag HD200 Red", 0},
    {1344, 296, 128, Kind::graphic, Color::yellow, "SmartTag HD L Yellow", 0},
    {1346, 800, 480, Kind::graphic, Color::yellow, "SmartTag HD200 Yellow", 0},
    {1348, 264, 176, Kind::graphic, Color::red, "SmartTag HD T Red", 0},
    {1349, 264, 176, Kind::graphic, Color::yellow, "SmartTag HD T Yellow", 0},
    {1351, 648, 480, Kind::graphic, Color::mono, "SmartTag HD150", 0},
    {1353, 648, 480, Kind::graphic, Color::red, "SmartTag HD150 Red", 0},
    {1354, 648, 480, Kind::graphic, Color::red, "SmartTag HD150 Red", 0},
    {1370, 296, 128, Kind::graphic, Color::red, "SmartTag HD L Red", 0},
    {1371, 648, 480, Kind::graphic, Color::red, "SmartTag HD150 Red", 0},
    {1510, 0, 0, Kind::segment, Color::mono, "SmartTag E5 M", 0},
    {1626, 296, 152, Kind::graphic, Color::fourColor, "SmartTag Color 2.6", 1},
    {1627, 296, 128, Kind::graphic, Color::red, "SmartTag HD L Red", 0},
    {1628, 296, 128, Kind::graphic, Color::red, "SmartTag HD L Red", 0},
    {1639, 152, 152, Kind::graphic, Color::red, "SmartTag HD S Red", 0},
    {3145, 400, 300, Kind::graphic, Color::mono, "SmartTag HD110", 0},
    {3547, 648, 480, Kind::graphic, Color::red, "SmartTag HD150 Red", 0},
    {6275, 296, 128, Kind::graphic, Color::mono, "SmartTag HD L", 0},
    {3220, 152, 152, Kind::graphic, Color::mono, "SmartTag HD S", 0},
    {3227, 152, 152, Kind::graphic, Color::mono, "SmartTag HD S", 0},
    {3229, 152, 152, Kind::graphic, Color::mono, "SmartTag HD S", 0},
}};

const KnownProfile* findProfile(const std::uint16_t typeCode) {
  for (const auto& profile : knownProfiles) {
    if (profile.typeCode == typeCode) {
      return &profile;
    }
  }
  return nullptr;
}

bool isAsciiDigit(const char value) { return value >= '0' && value <= '9'; }

bool isAsciiAlphaNumeric(const char value) {
  return isAsciiDigit(value) || (value >= 'A' && value <= 'Z') || (value >= 'a' && value <= 'z');
}

std::uint32_t decimalField(const char* text, const std::size_t begin, const std::size_t end) {
  std::uint32_t value = 0;
  for (std::size_t index = begin; index < end; ++index) {
    value = value * 10U + static_cast<std::uint32_t>(text[index] - '0');
  }
  return value;
}

} // namespace

ParseResult parseBarcode(const char* barcode) {
  ParseResult result{};
  if (barcode == nullptr || std::strlen(barcode) != 17U) {
    result.error = ParseError::length;
    return result;
  }
  if (!isAsciiAlphaNumeric(barcode[0])) {
    result.error = ParseError::format;
    return result;
  }
  for (std::size_t index = 1; index < 17; ++index) {
    if (!isAsciiDigit(barcode[index])) {
      result.error = ParseError::format;
      return result;
    }
  }
  if (barcode[1] != '4') {
    result.error = ParseError::marker;
    return result;
  }

  std::uint16_t checksum = 0;
  for (std::size_t index = 0; index < 16; ++index) {
    checksum = static_cast<std::uint16_t>(checksum + static_cast<std::uint8_t>(barcode[index]));
  }
  if (checksum % 10U != static_cast<std::uint16_t>(barcode[16] - '0')) {
    result.error = ParseError::checksum;
    return result;
  }

  const std::uint32_t manufacturing = decimalField(barcode, 2, 7);
  const std::uint32_t serial = decimalField(barcode, 7, 12);
  const std::uint32_t manufacturingUnit = decimalField(barcode, 2, 4);
  const std::uint32_t manufacturingWeek = decimalField(barcode, 5, 7);
  if (manufacturingUnit >= 64U || manufacturingWeek >= 54U || manufacturing > 0xffffU ||
      serial >= 0xffffU) {
    result.error = ParseError::range;
    return result;
  }

  const std::uint32_t logicalPlid = (manufacturing << 16U) | serial;
  for (std::size_t index = 0; index < 4; ++index) {
    result.record.wirePlid[index] = static_cast<std::uint8_t>(logicalPlid >> (index * 8U));
  }
  std::memcpy(result.record.barcode.data(), barcode, 17);
  result.record.barcode[17] = '\0';
  result.record.typeCode = static_cast<std::uint16_t>(decimalField(barcode, 12, 16));
  result.record.fromBarcode = true;
  const bool profileDetected = detectProfile(result.record.typeCode, result.record.profile);
  (void)profileDetected;
  return result;
}

bool detectProfile(const std::uint16_t typeCode, Profile& profile) {
  const KnownProfile* known = findProfile(typeCode);
  if (known == nullptr) {
    profile = {};
    return false;
  }
  profile.kind = known->kind;
  profile.color = known->color;
  profile.width = known->width;
  profile.height = known->height;
  profile.imagePage =
      known->kind == Kind::graphic && known->imagePage == 0U ? 1U : known->imagePage;
  // Segment receivers support PP4 only.
  profile.pp16 = known->kind == Kind::graphic;
  profile.rotateClockwise = false;
  return true;
}

const char* profileName(const std::uint16_t typeCode) {
  const KnownProfile* known = findProfile(typeCode);
  return known == nullptr ? "Custom profile" : known->name;
}

const char* kindLabel(const Kind kind) {
  switch (kind) {
  case Kind::graphic:
    return "GRAPHIC";
  case Kind::segment:
    return "SEGMENT";
  case Kind::unknown:
    return "UNKNOWN";
  }
  return "UNKNOWN";
}

const char* colorLabel(const Color color) {
  switch (color) {
  case Color::mono:
    return "B/W";
  case Color::red:
    return "B/W/RED";
  case Color::yellow:
    return "B/W/YELLOW";
  case Color::fourColor:
    return "4-COLOR";
  }
  return "B/W";
}

const char* parseErrorLabel(const ParseError error) {
  switch (error) {
  case ParseError::none:
    return "";
  case ParseError::length:
    return "Check the length";
  case ParseError::format:
    return "Use the printed characters";
  case ParseError::marker:
    return "Not an ESL barcode";
  case ParseError::range:
    return "PLID is out of range";
  case ParseError::checksum:
    return "Checksum does not match";
  }
  return "Invalid value";
}

void formatPlid(const std::array<std::uint8_t, 4>& wirePlid, char destination[9]) {
  std::snprintf(destination, 9, "%02X%02X%02X%02X", wirePlid[3], wirePlid[2], wirePlid[1],
                wirePlid[0]);
}

esl::Frame makePingFrame(const std::array<std::uint8_t, 4>& wirePlid,
                         const esl::Encoding encoding) {
  std::array<std::uint8_t, 25> payload{};
  payload[0] = 0x97;
  payload[1] = 0x01;
  payload[2] = 0x00;
  payload[3] = 0x00;
  payload[4] = 0x00;
  for (std::size_t index = 5; index < payload.size(); ++index) {
    payload[index] = 0x01;
  }
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

esl::Frame makeWakeFrame(const std::array<std::uint8_t, 4>& wirePlid, const esl::Encoding encoding,
                         const bool requestAcknowledgement) {
  if (requestAcknowledgement) {
    return makePingFrame(wirePlid, encoding);
  }
  std::array<std::uint8_t, 27> payload{};
  payload[0] = 0x17;
  payload[1] = 0x01;
  payload[2] = 0x00;
  payload[3] = 0x00;
  payload[4] = 0x00;
  for (std::size_t index = 5; index < payload.size(); ++index) {
    payload[index] = 0x01;
  }
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

esl::Frame makeBlinkFrame(const std::array<std::uint8_t, 4>& wirePlid,
                          const std::uint16_t durationSeconds, const esl::Encoding encoding) {
  // 0x49 retains page 1 and the green LED bit while clearing 0x80, the
  // persistent-duration bit used by 0xc9. Duration is a big-endian word.
  const std::array<std::uint8_t, 6> payload{0x06,
                                            0x49,
                                            0,
                                            0,
                                            static_cast<std::uint8_t>(durationSeconds >> 8U),
                                            static_cast<std::uint8_t>(durationSeconds & 0xffU)};
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

esl::Frame makeImageParametersFrame(const std::array<std::uint8_t, 4>& wirePlid,
                                    const std::uint16_t byteCount, const std::uint8_t compression,
                                    const std::uint8_t page, const std::uint16_t width,
                                    const std::uint16_t height, const esl::Encoding encoding) {
  const std::array<std::uint8_t, 27> payload{0x34,
                                             0,
                                             0,
                                             0,
                                             0x05,
                                             static_cast<std::uint8_t>(byteCount >> 8U),
                                             static_cast<std::uint8_t>(byteCount & 0xffU),
                                             0,
                                             compression,
                                             page,
                                             static_cast<std::uint8_t>(width >> 8U),
                                             static_cast<std::uint8_t>(width & 0xffU),
                                             static_cast<std::uint8_t>(height >> 8U),
                                             static_cast<std::uint8_t>(height & 0xffU),
                                             0,
                                             0,
                                             0,
                                             0,
                                             0,
                                             0,
                                             0x88,
                                             0,
                                             0,
                                             0,
                                             0,
                                             0,
                                             0};
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

esl::Frame makeImageDataFrame(const std::array<std::uint8_t, 4>& wirePlid,
                              const std::uint16_t index, const std::uint8_t data[20],
                              const esl::Encoding encoding) {
  std::array<std::uint8_t, 27> payload{0x34,
                                       0,
                                       0,
                                       0,
                                       0x20,
                                       static_cast<std::uint8_t>(index >> 8U),
                                       static_cast<std::uint8_t>(index & 0xffU)};
  for (std::size_t byte = 0; byte < 20; ++byte) {
    payload[7 + byte] = data[byte];
  }
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

esl::Frame makeRefreshFrame(const std::array<std::uint8_t, 4>& wirePlid,
                            const esl::Encoding encoding) {
  constexpr std::array<std::uint8_t, 23> payload{0x34, 0, 0, 0, 0x01};
  return esl::makeFrame(0x85, wirePlid, payload.data(), payload.size(), encoding);
}

} // namespace tagtinker::target
