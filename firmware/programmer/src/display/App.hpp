// SPDX-License-Identifier: GPL-3.0-only

#pragma once

#include "BleBridge.hpp"
#include <etag/display/BroadcastProtocol.hpp>
#include <etag/display/ImageCodec.hpp>
#include "ImageLibrary.hpp"
#include "InfraredTransmitter.hpp"
#include <etag/display/TargetProtocol.hpp>
#include "TargetRenderer.hpp"
#include "TargetStore.hpp"

#include <M5Cardputer.h>
#include <M5GFX.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace tagtinker {

enum class Screen : std::uint8_t {
  splash,
  mainMenu,
  broadcast,
  graphicPage,
  segmentPage,
  diagnostics,
  sending,
  result,
  target,
  targetBarcode,
  targetProfileKind,
  targetProfileSize,
  targetProfileWidth,
  targetProfileHeight,
  targetProfileOrientation,
  targetProfileColor,
  targetActions,
  targetText,
  targetName,
  targetTextStyle,
  targetImage,
  targetImageSend,
  targetBlink,
  targetDetails,
  targetRemove,
  webTransport,
  webUi,
};

class App final {
public:
  App();

  bool begin();
  void resume();
  bool consoleRequested() const { return consoleRequested_; }
  void update();

private:
  struct MenuItem {
    char key;
    const char* label;
    Screen destination;
  };

  static constexpr std::uint32_t splashDurationMs = 1250;
  static const std::array<MenuItem, 2> menuItems_;
  static const std::array<MenuItem, 3> broadcastItems_;

  bool initialized_ = false;
  bool consoleRequested_ = false;
  bool useBluetooth_ = false;
  M5Canvas canvas_;
  InfraredTransmitter transmitter_;
  TargetStore targets_;
  ImageLibrary images_;
  BleBridge bleBridge_{targets_, transmitter_};
  Screen screen_ = Screen::splash;
  broadcast::Action action_ = broadcast::Action::graphicPage;
  broadcast::GraphicDuration graphicDuration_ = broadcast::GraphicDuration::seconds15;
  broadcast::SegmentDuration segmentDuration_ = broadcast::SegmentDuration::seconds8;
  InfraredTransmitter::Status result_ = InfraredTransmitter::Status::idle;
  enum class Transmission : std::uint8_t {
    broadcast,
    targetBlink,
    targetText,
    targetImage,
  };
  Transmission transmission_ = Transmission::broadcast;
  target::Record pendingTarget_{};
  image::Encoded imageData_{};
  std::array<char, 65> input_{};
  target::ParseError inputError_ = target::ParseError::none;
  std::size_t inputLength_ = 0;
  std::size_t targetListPage_ = 0;
  std::size_t profileSizePage_ = 0;
  std::size_t imageListPage_ = 0;
  std::size_t selectedImage_ = 0;
  std::size_t blinkDurationIndex_ = 1;
  int selectedTarget_ = -1;
  bool composeError_ = false;
  bool imageComposeError_ = false;
  enum class DimensionError : std::uint8_t {
    none,
    range,
    alignment,
    area,
  };
  DimensionError dimensionError_ = DimensionError::none;
  std::uint16_t customWidth_ = 0;
  std::uint16_t customHeight_ = 0;
  bool customProfileSize_ = false;
  bool editingProfile_ = false;
  bool saveError_ = false;
  std::uint8_t page_ = 0;
  std::uint8_t targetPage_ = 1;
  render::TextStyle textStyle_{};
  std::uint8_t textStylePage_ = 0;
  std::uint8_t drawnProgress_ = 0;
  std::uint32_t screenEnteredAt_ = 0;
  std::uint32_t progressDrawnAt_ = 0;

  void open(Screen screen);
  void handleMainMenu();
  void handleBroadcastMenu();
  void handlePageEditor();
  void handleDiagnostics();
  void handleSending();
  void handleResult();
  void handleTargetMenu();
  void handleTargetInput();
  void handleTargetProfileKind();
  void handleTargetProfileSize();
  void handleTargetProfileDimension(bool width);
  void handleTargetProfileOrientation();
  void handleTargetProfileColor();
  void handleTargetActions();
  void handleTargetText();
  void handleTargetName();
  void drawTargetName();
  void handleTargetTextStyle();
  void handleTargetImage();
  void handleTargetImageSend();
  void handleTargetBlink();
  void handleTargetDetails();
  void handleTargetRemove();
  void handleWebUi();
  void startBroadcast();
  void startTargetBlink();
  void startTargetImage(Transmission content);
  bool prepareTextImage();
  bool prepareFileImage(const char* path);
  [[nodiscard]] bool savePendingTarget();
  void resetInput();

  void drawSplash();
  void drawMainMenu();
  void drawBroadcastMenu();
  void drawPageEditor();
  void drawDiagnostics();
  void drawSending();
  void drawResult();
  void drawTargetMenu();
  void drawTargetInput();
  void drawTargetProfileKind();
  void drawTargetProfileSize();
  void drawTargetProfileDimension(bool width);
  void drawTargetProfileOrientation();
  void drawTargetProfileColor();
  void drawTargetActions();
  void drawTargetText();
  void drawTargetTextStyle();
  void drawTargetImage();
  void drawTargetImageSend();
  void drawTargetBlink();
  void drawTargetDetails();
  void drawTargetRemove();
  void drawWebUi();
  void drawHeader(const char* title);
  void drawFitted(const char* text, std::int32_t x, std::int32_t y, std::int32_t width);
  void drawMenuItem(const MenuItem& item, std::int32_t y);
  void drawMenuLine(char key, const char* label, std::int32_t y);
  void drawFooter(const char* text, std::int32_t y = 125);
  void formatAction(char* destination, std::size_t capacity) const;
  void formatTargetLabel(const target::Record& record, char* destination,
                         std::size_t capacity) const;
  [[nodiscard]] const char* selectedDurationLabel() const;
  [[nodiscard]] std::uint16_t selectedBlinkDuration() const;
  [[nodiscard]] const char* selectedBlinkDurationLabel() const;
  [[nodiscard]] const target::Record* selectedTarget() const;
  void present();
};

} // namespace tagtinker
