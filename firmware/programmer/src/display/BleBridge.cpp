#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only
#include "BleBridge.hpp"
#include <Arduino.h>
#include <cstring>
#include <cmath>
#include <climits>
#include <memory>
#include <cJSON.h>
#include <esp_heap_caps.h>
#include <mbedtls/base64.h>
#include <etag/display/Validation.hpp>
#include <etag/display/JsonEnvelope.hpp>
#include <esp_system.h>

#define NUS_SERVICE_UUID "6E400001-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_RX_UUID "6E400002-B5A3-F393-E0A9-E50E24DCCA9E"
#define NUS_TX_UUID "6E400003-B5A3-F393-E0A9-E50E24DCCA9E"

namespace tagtinker {
namespace {

std::string idOf(const target::Record& record) {
  char id[9]{};
  target::formatPlid(record.wirePlid, id);
  return id;
}

std::string jsonEscape(const std::string& s) {
  std::string out = "\"";
  for (char c : s) {
    if (c == '\"')
      out += "\\\"";
    else if (c == '\\')
      out += "\\\\";
    else if (static_cast<unsigned char>(c) < 32)
      out += ' ';
    else
      out += c;
  }
  return out + "\"";
}

std::string extractJsonString(const cJSON* json, const char* key) {
  const auto* value = cJSON_GetObjectItemCaseSensitive(json, key);
  return cJSON_IsString(value) ? value->valuestring : "";
}
int extractJsonInt(const cJSON* json, const char* key, int fallback = 0) {
  const auto* value = cJSON_GetObjectItemCaseSensitive(json, key);
  return cJSON_IsNumber(value) ? static_cast<int>(value->valuedouble) : fallback;
}
bool validCommand(const cJSON* json) {
  if (!cJSON_IsObject(json)) return false;
  unsigned count = 0;
  for (const cJSON* field = json->child; field; field = field->next) {
    if (++count > 12 || !field->string) return false;
    for (const cJSON* other = field->next; other; other = other->next)
      if (other->string && std::strcmp(other->string, field->string) == 0) return false;
    if (cJSON_IsNumber(field)) {
      const double n = field->valuedouble;
      if (!std::isfinite(n) || n < INT_MIN || n > INT_MAX || std::floor(n) != n) return false;
    } else if (!cJSON_IsString(field)) return false;
    const bool numeric = std::strcmp(field->string, "width") == 0 ||
        std::strcmp(field->string, "height") == 0 || std::strcmp(field->string, "color") == 0 ||
        std::strcmp(field->string, "page") == 0 || std::strcmp(field->string, "len") == 0 ||
        std::strcmp(field->string, "comp") == 0 || std::strcmp(field->string, "offset") == 0;
    if (numeric != bool(cJSON_IsNumber(field))) return false;
  }
  return cJSON_IsString(cJSON_GetObjectItemCaseSensitive(json, "cmd"));
}
bool base64Decode(const std::string& in, std::vector<std::uint8_t>& out) {
  if (in.empty() || in.size() > 320 || in.size() % 4 != 0) return false;
  out.resize(240);
  std::size_t length = 0;
  if (mbedtls_base64_decode(out.data(), out.size(), &length,
      reinterpret_cast<const unsigned char*>(in.data()), in.size()) != 0) return false;
  out.resize(length);
  return length > 0;
}

} // namespace

BleBridge::BleBridge(TargetStore& targets, InfraredTransmitter& transmitter)
    : targets_(targets), transmitter_(transmitter) {
  char suffix[7];
  std::snprintf(suffix, sizeof(suffix), "%04X", static_cast<unsigned>(ESP.getEfuseMac() & 0xffff));
  deviceName_ = std::string("etag Cardputer ") + suffix;
}

BleBridge::~BleBridge() { stop(); }

bool BleBridge::begin(bool bluetooth) {
  if (active_) return true;
  bleMode_ = bluetooth;
  rxBleStream_.reset();
  rxSerialStream_.reset();
  resetUpload();
  staged_ = {};
  // A session owns exactly one transport; USB avoids the BLE heap allocation.
  if (bluetooth && !bleInitialized_) {
    BLEDevice::init(deviceName_);
    server_ = BLEDevice::createServer();
    if (!server_) return false;
    server_->setCallbacks(this);
    BLEService* service = server_->createService(NUS_SERVICE_UUID);
    if (!service) return false;
    txCharacteristic_ = service->createCharacteristic(NUS_TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);
    txCharacteristic_->addDescriptor(new BLE2902());
    rxCharacteristic_ = service->createCharacteristic(NUS_RX_UUID,
        BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR);
    rxCharacteristic_->setCallbacks(this);
    service->start();
    BLEAdvertising* advertising = BLEDevice::getAdvertising();
    advertising->addServiceUUID(NUS_SERVICE_UUID);
    advertising->setScanResponse(true);
    advertising->setMinPreferred(0x06);
    advertising->setMinPreferred(0x12);
    bleInitialized_ = true;
  }
  active_ = true;
  if (bluetooth) BLEDevice::startAdvertising();
  return true;
}

void BleBridge::stop() {
  active_ = false;
  if (bleInitialized_) {
    BLEDevice::stopAdvertising();
    if (deviceConnected_) server_->disconnect(server_->getConnId());
  }
  deviceConnected_ = false;
  oldDeviceConnected_ = false;
  portENTER_CRITICAL(&rxMux_);
  rxBleSize_ = 0;
  rxBleOverflow_ = false;
  portEXIT_CRITICAL(&rxMux_);
  rxBleStream_.reset();
  rxSerialStream_.reset();
  // App exits this mode only after any transfer finishes/cancels.
  if (transmitter_.status() != InfraredTransmitter::Status::sending) {
    resetUpload();
    staged_ = {};
  }
}

void BleBridge::onConnect(BLEServer* server) {
  if (active_ && bleMode_) deviceConnected_ = true;
  else server->disconnect(server->getConnId());
}
void BleBridge::onDisconnect(BLEServer* /*server*/) { deviceConnected_ = false; }
void BleBridge::onWrite(BLECharacteristic* characteristic) {
  if (!active_ || !bleMode_) return;
  const std::string value = characteristic->getValue();
  portENTER_CRITICAL(&rxMux_);
  if (value.size() > rxBleQueue_.size() - rxBleSize_) rxBleOverflow_ = true;
  else {
    std::memcpy(rxBleQueue_.data() + rxBleSize_, value.data(), value.size());
    rxBleSize_ += value.size();
  }
  portEXIT_CRITICAL(&rxMux_);
}
void BleBridge::resetUpload() {
  std::vector<std::uint8_t>().swap(artworkBuffer_);
  expectedArtworkSize_ = 0;
}
void BleBridge::receive(char c, BoundedLine<>& stream) {
  const auto event = stream.push(c);
  if (event == BoundedLine<>::Event::ready && stream.line()[0]) processLine(stream.line());
  else if (event == BoundedLine<>::Event::rejected) {
    resetUpload();
    sendError("Command too long or invalid. Restart upload.");
  }
}
void BleBridge::update() {
  if (!active_) return;
  if (bleMode_) {
    std::array<char, 2048> incoming{};
    portENTER_CRITICAL(&rxMux_);
    const auto length = rxBleSize_;
    const bool overflow = rxBleOverflow_;
    std::memcpy(incoming.data(), rxBleQueue_.data(), length);
    rxBleSize_ = 0;
    rxBleOverflow_ = false;
    portEXIT_CRITICAL(&rxMux_);
    if (overflow) {
      // A lost byte destroys framing. End the session instead of interpreting a suffix.
      sendError("Receive queue full. Reopen browser connection.");
      stop();
      return;
    }
    for (std::size_t i = 0; i < length; ++i) receive(incoming[i], rxBleStream_);
    if (!deviceConnected_ && oldDeviceConnected_) {
      rxBleStream_.reset();
      resetUpload();
      server_->startAdvertising();
    }
    if (deviceConnected_ && !oldDeviceConnected_) sendState();
    oldDeviceConnected_ = deviceConnected_.load();
  } else {
    for (unsigned n = 0; n < 512 && Serial.available(); ++n)
      receive(static_cast<char>(Serial.read()), rxSerialStream_);
  }

  const auto status = transmitter_.status();
  const std::uint8_t prog = transmitter_.progress();
  const std::uint32_t now = millis();
  const bool isSending = (status == InfraredTransmitter::Status::sending);
  const bool statusChanged =
      (status != lastReportedStatus_) || (transmitter_.fast() != lastReportedFast_);
  const bool progressEligible =
      isSending && (prog != lastReportedProgress_) &&
      (prog == 0 || prog == 100 ||
       (prog >= lastReportedProgress_ + 2 && now - lastProgressSentAt_ >= 80));

  if (statusChanged || progressEligible) {
    lastReportedStatus_ = status;
    lastReportedFast_ = transmitter_.fast();
    lastReportedProgress_ = prog;
    if (progressEligible) {
      lastProgressSentAt_ = now;
    }
    const char* statusStr = status == InfraredTransmitter::Status::sending     ? "sending"
                            : status == InfraredTransmitter::Status::succeeded ? "sent"
                            : status == InfraredTransmitter::Status::error     ? "error"
                            : status == InfraredTransmitter::Status::cancelled ? "cancelled"
                            : staged_.valid                                    ? "loaded"
                                                                               : "idle";
    char statusJson[96];
    std::snprintf(statusJson, sizeof(statusJson),
                  "{\"status\":\"%s\",\"progress\":%u,\"speed\":\"%s\"}", statusStr, prog,
                  transmitter_.fast() ? "fast" : "reliable");
    sendEvent("status", statusJson);
  }
}

void BleBridge::sendLine(const std::string& line) {
  const std::string fullLine = line + "\n";

  if (!bleMode_) Serial.print(fullLine.c_str());

  // Send over BLE in chunks of <= 20 bytes (safe for default MTU 23)
  if (deviceConnected_ && txCharacteristic_ != nullptr) {
    constexpr std::size_t maxChunk = 20;
    for (std::size_t i = 0; i < fullLine.length(); i += maxChunk) {
      const std::string chunk = fullLine.substr(i, maxChunk);
      txCharacteristic_->setValue(chunk);
      txCharacteristic_->notify();
      delay(4);
    }
  }
}

void BleBridge::sendEvent(const std::string& event, const std::string& payloadJson) {
  std::string msg = "{\"event\":\"" + event + "\"";
  if (!payloadJson.empty()) {
    msg += ",\"data\":" + payloadJson;
  }
  msg += "}";
  sendLine(msg);
}

void BleBridge::sendError(const std::string& message) {
  sendLine("{\"event\":\"error\",\"message\":" + jsonEscape(message) + "}");
}

const target::Record* BleBridge::findTargetById(const std::string& id) {
  for (std::size_t i = 0; i < targets_.size(); ++i) {
    const auto* r = targets_.get(i);
    if (idOf(*r) == id)
      return r;
  }
  return nullptr;
}

void BleBridge::sendState() {
  const auto status = transmitter_.status();
  const char* label = status == InfraredTransmitter::Status::sending     ? "sending"
                      : status == InfraredTransmitter::Status::succeeded ? "sent"
                      : status == InfraredTransmitter::Status::error     ? "error"
                      : status == InfraredTransmitter::Status::cancelled ? "cancelled"
                      : staged_.valid                                    ? "loaded"
                                                                         : "idle";

  std::string json = "{\"device\":" + jsonEscape(deviceName_) + ",\"status\":\"" +
                     std::string(label) + "\"" +
                     ",\"speed\":" + jsonEscape(transmitter_.fast() ? "fast" : "reliable") +
                     ",\"progress\":" + std::to_string(transmitter_.progress()) + ",\"tags\":[";

  for (std::size_t i = 0; i < targets_.size(); ++i) {
    const auto& r = *targets_.get(i);
    if (i > 0)
      json += ",";
    json += "{\"id\":" + jsonEscape(idOf(r)) + ",\"name\":" + jsonEscape(r.name.data()) +
            ",\"barcode\":" + jsonEscape(r.barcode.data()) +
            ",\"model\":" + jsonEscape(target::profileName(r.typeCode)) +
            ",\"width\":" + std::to_string(r.profile.width) +
            ",\"height\":" + std::to_string(r.profile.height) +
            ",\"rotate\":" + (r.profile.rotateClockwise ? "true" : "false") +
            ",\"color\":" + std::to_string(static_cast<unsigned>(r.profile.color)) +
            ",\"graphic\":" + (r.profile.kind == target::Kind::graphic ? "true" : "false") +
            ",\"page\":" + std::to_string(r.profile.imagePage) +
            ",\"transport\":\"" + (r.profile.pp16 ? "ir_pp16" : "ir_pp4") +
            "\",\"firmwareRead\":false,\"firmwareWrite\":false,\"acknowledged\":false}";
  }
  json += "]}";
  sendEvent("state", json);
}

void BleBridge::processLine(const std::string& line) {
  if (!flatJsonEnvelope(line)) { sendError("Expected a flat command object."); return; }
  const char* end = nullptr;
  std::unique_ptr<cJSON, decltype(&cJSON_Delete)> doc(
      cJSON_ParseWithOpts(line.c_str(), &end, true), cJSON_Delete);
  if (!doc || !validCommand(doc.get())) { sendError("Invalid command JSON."); return; }
  const std::string cmd = extractJsonString(doc.get(), "cmd");
  if (cmd.empty())
    return;

  if (cmd == "getState") {
    sendState();
  } else if (cmd == "setSpeed") {
    const auto speed = extractJsonString(doc.get(), "speed");
    if (transmitter_.status() == InfraredTransmitter::Status::sending ||
        (speed != "fast" && speed != "reliable")) {
      sendError("Wait for transmission to finish and choose Fast or Reliable.");
      return;
    }
    transmitter_.setFast(speed == "fast");
    sendEvent("speedChanged", "{\"speed\":" + jsonEscape(speed) + "}");
  } else if (cmd == "saveTag") {
    const std::string barcode = extractJsonString(doc.get(), "barcode");
    const std::string name = extractJsonString(doc.get(), "name");
    const std::string existingId = extractJsonString(doc.get(), "id");

    target::Record record{};
    if (!existingId.empty()) {
      const auto* existing = findTargetById(existingId);
      if (!existing) {
        sendError("Target not found");
        return;
      }
      record = *existing;
    } else {
      auto parsed = target::parseBarcode(barcode.c_str());
      if (!parsed.ok()) {
        sendError(target::parseErrorLabel(parsed.error));
        return;
      }
      record = parsed.record;
      for (std::size_t i = 0; i < targets_.size(); ++i) {
        if (targets_.get(i)->wirePlid == record.wirePlid) {
          record = *targets_.get(i);
        }
      }
      if (record.profile.kind == target::Kind::unknown) {
        const int w = extractJsonInt(doc.get(), "width", 0);
        const int h = extractJsonInt(doc.get(), "height", 0);
        const int c = extractJsonInt(doc.get(), "color", 3);
        if (w < 8 || h < 8 || w > 2048 || h > 2048 || c < 0 || c > 3) {
          sendError("Invalid display dimensions or palette."); return;
        }
        record.profile = {target::Kind::graphic,
                          static_cast<target::Color>(c),
                          static_cast<std::uint16_t>(w),
                          static_cast<std::uint16_t>(h),
                          1,
                          true,
                          false};
        record.profileOverridden = true;
      }
    }
    if (!name.empty()) {
      std::snprintf(record.name.data(), record.name.size(), "%s", name.c_str());
    }
    if (!target::validRecord(record)) { sendError("Invalid IR device profile."); return; }
    if (targets_.upsert(record) < 0) {
      sendError("Could not save device: storage failed or nine slots full.");
      return;
    }
    sendEvent("tagSaved", "{\"id\":" + jsonEscape(idOf(record)) + "}");
    sendState();
  } else if (cmd == "renameTag") {
    const std::string id = extractJsonString(doc.get(), "id");
    const std::string name = extractJsonString(doc.get(), "name");
    const auto* r = findTargetById(id);
    if (!r) {
      sendError("Tag not found.");
      return;
    }
    target::Record record = *r;
    std::snprintf(record.name.data(), record.name.size(), "%s", name.c_str());
    if (targets_.upsert(record) < 0) { sendError("Could not save device name."); return; }
    sendEvent("tagRenamed", "{\"id\":" + jsonEscape(id) + "}");
    sendState();
  } else if (cmd == "removeTag") {
    const std::string id = extractJsonString(doc.get(), "id");
    for (std::size_t i = 0; i < targets_.size(); ++i) {
      if (idOf(*targets_.get(i)) == id) {
        if (!targets_.erase(i)) { sendError("Could not remove saved device."); return; }
        sendEvent("tagRemoved", "{\"id\":" + jsonEscape(id) + "}");
        sendState();
        return;
      }
    }
    sendError("Tag not found.");
  } else if (cmd == "blink") {
    const std::string id = extractJsonString(doc.get(), "id");
    const auto* r = findTargetById(id);
    if (!r || r->profile.kind != target::Kind::graphic) {
      sendError("Graphic tag not found.");
      return;
    }
    InfraredTransmitter::Plan plan{};
    plan.steps[0] = {target::makeBlinkFrame(r->wirePlid, 10, esl::Encoding::pp4), 200, 1};
    plan.size = 1;
    if (!transmitter_.start(plan)) {
      sendError("Could not start blink transmission.");
      return;
    }
    sendEvent("blinkStarted");
  } else if (cmd == "cancel") {
    transmitter_.cancel();
    sendEvent("cancelled");
  } else if (cmd == "startArt") {
    if (transmitter_.status() == InfraredTransmitter::Status::sending) {
      sendError("Transmission in progress. Cancel or wait.");
      return;
    }
    resetUpload();
    staged_ = {};
    const std::string id = extractJsonString(doc.get(), "id");
    const auto* r = findTargetById(id);
    if (!r || !target::capabilities(*r).displayUpdate) {
      sendError("Select a saved graphic tag first.");
      return;
    }
    pendingRecord_ = *r;
    pendingPage_ = static_cast<unsigned>(extractJsonInt(doc.get(), "page", r->profile.imagePage));
    expectedArtworkSize_ = static_cast<std::size_t>(extractJsonInt(doc.get(), "len", 0));
    pendingCompression_ = extractJsonInt(doc.get(), "comp", -1);

    const std::size_t rawSize = (std::size_t(r->profile.width) * r->profile.height *
                                     (r->profile.color == target::Color::mono ? 1U : 2U) +
                                 7U) /
                                8U;
    if (pendingPage_ > 7 || expectedArtworkSize_ == 0 ||
        (pendingCompression_ != -1 && pendingCompression_ != 0 && pendingCompression_ != 2) ||
        (pendingCompression_ == -1
             ? expectedArtworkSize_ != rawSize || rawSize > 96000U
             : expectedArtworkSize_ > 65520U || expectedArtworkSize_ % 20U != 0)) {
      expectedArtworkSize_ = 0;
      sendError("Invalid artwork size, page, or compression.");
      return;
    }
    artworkBuffer_.clear();
    if (heap_caps_get_largest_free_block(MALLOC_CAP_8BIT) < expectedArtworkSize_ + 16384U ||
        ESP.getFreeHeap() < expectedArtworkSize_ * 2U + 32768U) {
      resetUpload();
      sendError("Insufficient memory. Use USB after reboot or a simpler image."); return;
    }
    artworkBuffer_.reserve(expectedArtworkSize_);
    sendEvent("artReady", "{\"len\":" + std::to_string(expectedArtworkSize_) + "}");
  } else if (cmd == "artChunk") {
    const std::string dataB64 = extractJsonString(doc.get(), "data");
    std::vector<std::uint8_t> chunk;
    const int offset = extractJsonInt(doc.get(), "offset", -1);
    if (transmitter_.status() == InfraredTransmitter::Status::sending ||
        expectedArtworkSize_ == 0 || dataB64.empty() || dataB64.size() > 320 || offset < 0 ||
        static_cast<std::size_t>(offset) != artworkBuffer_.size() ||
        !base64Decode(dataB64, chunk) || chunk.empty() ||
        chunk.size() > expectedArtworkSize_ - artworkBuffer_.size()) {
      resetUpload();
      sendError("Invalid artwork chunk. Restart the upload.");
      return;
    }
    artworkBuffer_.insert(artworkBuffer_.end(), chunk.begin(), chunk.end());
    char ack[64];
    std::snprintf(ack, sizeof(ack), "{\"received\":%u,\"expected\":%u}",
                  static_cast<unsigned>(artworkBuffer_.size()),
                  static_cast<unsigned>(expectedArtworkSize_));
    sendEvent("chunkAck", ack);
  } else if (cmd == "finishArt") {
    if (transmitter_.status() == InfraredTransmitter::Status::sending) {
      sendError("Transmission in progress. Cancel or wait.");
      return;
    }
    if (artworkBuffer_.size() != expectedArtworkSize_ || expectedArtworkSize_ == 0) {
      sendError("Artwork buffer size mismatch.");
      return;
    }
    if (pendingCompression_ >= 0) {
      const std::size_t bits = std::size_t(pendingRecord_.profile.width) *
                               pendingRecord_.profile.height *
                               (pendingRecord_.profile.color == target::Color::mono ? 1U : 2U);
      if (!image::validate(artworkBuffer_, pendingCompression_, bits)) {
        sendError("Invalid pre-encoded artwork size.");
        return;
      }
      staged_.record = pendingRecord_;
      staged_.data = std::move(artworkBuffer_);
      staged_.width = pendingRecord_.profile.width;
      staged_.height = pendingRecord_.profile.height;
      staged_.compression = static_cast<std::uint8_t>(pendingCompression_);
      staged_.page = static_cast<std::uint8_t>(pendingPage_);
      staged_.valid = true;
      artworkBuffer_.clear();
      expectedArtworkSize_ = 0;

      sendEvent("artLoaded", "{\"status\":\"loaded\"}");
      sendEvent("status", "{\"status\":\"loaded\",\"progress\":0}");
    } else {
      const std::size_t bits = std::size_t(pendingRecord_.profile.width) *
                               pendingRecord_.profile.height *
                               (pendingRecord_.profile.color == target::Color::mono ? 1 : 2);
      if (artworkBuffer_.size() != (bits + 7) / 8 ||
          !image::encode(artworkBuffer_, bits, encoded_)) {
        sendError("Artwork encoding failed.");
        return;
      }
      staged_.record = pendingRecord_;
      staged_.data = std::move(encoded_.bytes);
      staged_.width = pendingRecord_.profile.width;
      staged_.height = pendingRecord_.profile.height;
      staged_.compression = encoded_.compression;
      staged_.page = static_cast<std::uint8_t>(pendingPage_);
      staged_.valid = true;
      artworkBuffer_.clear();
      expectedArtworkSize_ = 0;

      sendEvent("artLoaded", "{\"status\":\"loaded\"}");
      sendEvent("status", "{\"status\":\"loaded\",\"progress\":0}");
    }
  } else if (cmd == "transmit" || cmd == "repeat") {
    if (!transmitStaged()) {
      sendError("No artwork staged or transmitter busy.");
    }
  } else {
    sendError("Unknown display command.");
  }
}

bool BleBridge::transmitStaged() {
  if (!staged_.valid || staged_.data.empty() || !target::capabilities(staged_.record).displayUpdate)
    return false;
  if (transmitter_.status() == InfraredTransmitter::Status::sending)
    return false;

  const InfraredTransmitter::ImageTransfer transfer{
      staged_.record.wirePlid,
      staged_.data.data(),
      staged_.data.size(),
      staged_.width,
      staged_.height,
      staged_.compression,
      staged_.page,
      staged_.record.profile.pp16 ? esl::Encoding::pp16 : esl::Encoding::pp4};
  if (!transmitter_.startImage(transfer)) {
    sendError("IR transmitter failed to start.");
    return false;
  }
  sendEvent("artStarted");
  return true;
}

void BleBridge::clearStaged() {
  if (transmitter_.status() == InfraredTransmitter::Status::sending)
    return;
  staged_ = {};
  sendEvent("status", "{\"status\":\"idle\",\"progress\":0}");
}

} // namespace tagtinker

#endif // ETAG_CARDPUTER_ADV
