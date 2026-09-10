// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <etag/display/ImageCodec.hpp>
#include "InfraredTransmitter.hpp"
#include "TargetStore.hpp"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>

#include <etag/display/BoundedLine.hpp>
#include <array>
#include <atomic>
#include <string>
#include <vector>

namespace tagtinker {

class BleBridge final : public BLEServerCallbacks, public BLECharacteristicCallbacks {
public:
  BleBridge(TargetStore& targets, InfraredTransmitter& transmitter);
  ~BleBridge();

  bool begin(bool bluetooth = false);
  void update();
  void stop();

  struct StagedArtwork {
    target::Record record{};
    std::vector<std::uint8_t> data{};
    std::uint16_t width = 0;
    std::uint16_t height = 0;
    std::uint8_t compression = 0;
    std::uint8_t page = 0;
    bool valid = false;
  };

  [[nodiscard]] bool active() const { return active_; }
  [[nodiscard]] bool connected() const { return deviceConnected_; }
  [[nodiscard]] const std::string& deviceName() const { return deviceName_; }
  [[nodiscard]] bool hasStaged() const { return staged_.valid; }
  [[nodiscard]] const StagedArtwork& staged() const { return staged_; }
  bool transmitStaged();
  void clearStaged();

private:
  TargetStore& targets_;
  InfraredTransmitter& transmitter_;

  BLEServer* server_ = nullptr;
  BLECharacteristic* txCharacteristic_ = nullptr;
  BLECharacteristic* rxCharacteristic_ = nullptr;

  std::string deviceName_ = "TagTinker ADV";
  std::atomic<bool> active_{false};
  bool bleMode_ = false;
  bool bleInitialized_ = false;
  std::atomic<bool> deviceConnected_{false};
  bool oldDeviceConnected_ = false;

  portMUX_TYPE rxMux_ = portMUX_INITIALIZER_UNLOCKED;
  std::array<char, 2048> rxBleQueue_{};
  std::size_t rxBleSize_ = 0;
  bool rxBleOverflow_ = false;
  BoundedLine<> rxBleStream_;
  BoundedLine<> rxSerialStream_;
  void receive(char c, BoundedLine<>& stream);
  void resetUpload();

  std::vector<std::uint8_t> artworkBuffer_;
  std::size_t expectedArtworkSize_ = 0;
  int pendingCompression_ = -1;
  target::Record pendingRecord_{};
  unsigned pendingPage_ = 1;
  image::Encoded encoded_{};
  std::uint8_t lastReportedProgress_ = 0;
  bool lastReportedFast_ = false;
  std::uint32_t lastProgressSentAt_ = 0;
  InfraredTransmitter::Status lastReportedStatus_ = InfraredTransmitter::Status::idle;
  StagedArtwork staged_{};

  void onConnect(BLEServer* server) override;
  void onDisconnect(BLEServer* server) override;

  void onWrite(BLECharacteristic* characteristic) override;

  void sendLine(const std::string& line);
  void processLine(const std::string& line);
  void sendState();
  void sendEvent(const std::string& event, const std::string& payloadJson = "");
  void sendError(const std::string& message);
  const target::Record* findTargetById(const std::string& id);
};

} // namespace tagtinker
