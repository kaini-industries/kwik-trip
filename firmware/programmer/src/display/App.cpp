#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only

#include "App.hpp"

#include "TargetRenderer.hpp"
#include "TargetEditing.hpp"
#include "Theme.hpp"

#include <SD.h>

#include <cstdio>
#include <cstring>

namespace tagtinker {
namespace {

constexpr std::int32_t screenWidth = 240;
constexpr std::int32_t screenHeight = 135;

struct SizePreset {
  std::uint16_t width;
  std::uint16_t height;
  bool rotateClockwise = false;
  const char* label;
};

constexpr std::array<SizePreset, 13> sizePresets{{
    {152, 152, false, "1.6  / 152 x 152"},
    {208, 112, false, "2.2  / 208 x 112"},
    {296, 152, false, "2.6  / 296 x 152"},
    {296, 128, false, "2.9  / 296 x 128"},
    {400, 300, false, "4.2  / 400 x 300"},
    {522, 152, false, "4.3  / 522 x 152"},
    {792, 272, false, "5.8  / 792 x 272"},
    {800, 480, false, "7.x  / 800 x 480"},
    {172, 72, false, "Legacy / 172 x 72"},
    {320, 140, false, "Legacy / 320 x 140"},
    {320, 192, false, "Legacy / 320 x 192"},
    {264, 176, false, "Legacy / 264 x 176"},
    {648, 480, false, "Legacy / 648 x 480"},
}};

constexpr std::size_t targetRowsPerPage = 3;
constexpr std::size_t sizeRowsPerPage = 4;
constexpr std::size_t imageRowsPerPage = 4;
constexpr std::size_t profileSizeChoices = sizePresets.size() + 1U;
constexpr std::uint16_t minimumCustomDimension = 8;
constexpr std::uint16_t maximumCustomDimension = 2048;
constexpr std::uint32_t maximumCustomPixels = 384000;

struct BlinkDuration {
  std::uint16_t seconds;
  const char* label;
};

constexpr std::array<BlinkDuration, 5> blinkDurations{{
    {1, "1 second"},
    {10, "10 seconds"},
    {30, "30 seconds"},
    {60, "1 minute"},
    {120, "2 minutes"},
}};

std::uint16_t displayWidth(const target::Profile& profile) {
  return profile.rotateClockwise ? profile.height : profile.width;
}

std::uint16_t displayHeight(const target::Profile& profile) {
  return profile.rotateClockwise ? profile.width : profile.height;
}

const char* fontLabel(const render::TextFont font) {
  constexpr std::array<const char*, 5> labels{"Sans", "Sans bold", "Serif", "Mono", "Rounded"};
  return labels[static_cast<std::size_t>(font)];
}

const char* sizeLabel(const render::TextSize size) {
  constexpr std::array<const char*, 4> labels{"Auto fit", "Small", "Medium", "Large"};
  return labels[static_cast<std::size_t>(size)];
}

const char* alignLabel(const render::TextAlign align) {
  constexpr std::array<const char*, 3> labels{"Left", "Center", "Right"};
  return labels[static_cast<std::size_t>(align)];
}

const char* verticalLabel(const render::TextVertical vertical) {
  constexpr std::array<const char*, 3> labels{"Top", "Middle", "Bottom"};
  return labels[static_cast<std::size_t>(vertical)];
}

const char* colorLabel(const render::TextColor color) {
  constexpr std::array<const char*, 4> labels{"Black", "White", "Red", "Yellow"};
  return labels[static_cast<std::size_t>(color)];
}

bool supportsTextColor(const target::Color palette, const render::TextColor color) {
  return color == render::TextColor::black || color == render::TextColor::white ||
         (color == render::TextColor::red &&
          (palette == target::Color::red || palette == target::Color::fourColor)) ||
         (color == render::TextColor::yellow &&
          (palette == target::Color::yellow || palette == target::Color::fourColor));
}

render::TextColor nextTextColor(const target::Color palette, const render::TextColor current,
                                const render::TextColor excluded) {
  auto candidate = current;
  for (std::size_t attempt = 0; attempt < 4U; ++attempt) {
    candidate = static_cast<render::TextColor>((static_cast<std::uint8_t>(candidate) + 1U) % 4U);
    if (candidate != excluded && supportsTextColor(palette, candidate)) {
      return candidate;
    }
  }
  return current;
}

} // namespace

const std::array<App::MenuItem, 3> App::menuItems_{{
    {'1', "IR devices", Screen::target},
    {'3', "Browser editor", Screen::webTransport},
    {'4', "OpenEPaperLink", Screen::oepl},
}};

const std::array<App::MenuItem, 3> App::broadcastItems_{{
    {'1', "Graphic page", Screen::graphicPage},
    {'2', "Segment page", Screen::segmentPage},
    {'3', "Diagnostics", Screen::diagnostics},
}};

App::App() : canvas_(&M5Cardputer.Display) {}

bool App::begin() {
  if (initialized_) { resume(); return true; }
  canvas_.setColorDepth(8);
  if (!canvas_.createSprite(screenWidth, screenHeight)) return false;
  canvas_.setTextWrap(false);
  if (!transmitter_.begin()) { canvas_.deleteSprite(); return false; }
  targets_.begin();
  initialized_ = true;
  resume();
  return true;
}

void App::resume() {
  consoleRequested_ = false;
  open(Screen::mainMenu);
}

void App::update() {
  M5Cardputer.update();

  if (screen_ == Screen::oepl) {
    oepl_.update();
    if (oepl_.exitRequested()) open(Screen::mainMenu);
    return;
  }

  if (screen_ == Screen::splash) {
    const bool keyPressed = M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed();
    if (keyPressed || millis() - screenEnteredAt_ >= splashDurationMs) {
      open(Screen::mainMenu);
    }
    return;
  }

  if (screen_ == Screen::sending) {
    handleSending();
    return;
  }

  if (screen_ == Screen::webUi) {
    bleBridge_.update();
    handleWebUi();
    return;
  }

  if (!M5Cardputer.Keyboard.isChange() || !M5Cardputer.Keyboard.isPressed()) {
    return;
  }

  switch (screen_) {
  case Screen::mainMenu:
    handleMainMenu();
    break;
  case Screen::broadcast:
    handleBroadcastMenu();
    break;
  case Screen::graphicPage:
  case Screen::segmentPage:
    handlePageEditor();
    break;
  case Screen::diagnostics:
    handleDiagnostics();
    break;
  case Screen::result:
    handleResult();
    break;
  case Screen::target:
    handleTargetMenu();
    break;
  case Screen::targetBarcode:
    handleTargetInput();
    break;
  case Screen::targetProfileKind:
    handleTargetProfileKind();
    break;
  case Screen::targetProfileSize:
    handleTargetProfileSize();
    break;
  case Screen::targetProfileWidth:
    handleTargetProfileDimension(true);
    break;
  case Screen::targetProfileHeight:
    handleTargetProfileDimension(false);
    break;
  case Screen::targetProfileOrientation:
    handleTargetProfileOrientation();
    break;
  case Screen::targetProfileColor:
    handleTargetProfileColor();
    break;
  case Screen::targetActions:
    handleTargetActions();
    break;
  case Screen::targetDetails:
    handleTargetDetails();
    break;
  case Screen::targetRemove:
    handleTargetRemove();
    break;
  case Screen::targetText:
    handleTargetText();
    break;
  case Screen::targetName:
    handleTargetName();
    break;
  case Screen::targetTextStyle:
    handleTargetTextStyle();
    break;
  case Screen::targetImage:
    handleTargetImage();
    break;
  case Screen::targetImageSend:
    handleTargetImageSend();
    break;
  case Screen::targetBlink:
    handleTargetBlink();
    break;
  case Screen::webTransport:
    if (M5Cardputer.Keyboard.keysState().backspace) open(Screen::mainMenu);
    else if (M5Cardputer.Keyboard.isKeyPressed('1') || M5Cardputer.Keyboard.isKeyPressed('2')) {
      useBluetooth_ = M5Cardputer.Keyboard.isKeyPressed('2');
      // Release the on-device artwork before allocating browser buffers/BLE.
      imageData_ = {};
      open(Screen::webUi);
    }
    break;
  case Screen::webUi:
  case Screen::oepl:
    break;
  case Screen::splash:
  case Screen::sending:
    break;
  }
}

void App::open(const Screen screen) {
  if (screen_ == Screen::webUi && screen != Screen::webUi) {
    bleBridge_.stop();
  }
  screen_ = screen;
  screenEnteredAt_ = millis();
  if (screen == Screen::webUi) {
    bleBridge_.begin(useBluetooth_);
  }

  switch (screen) {
  case Screen::splash:
    drawSplash();
    break;
  case Screen::mainMenu:
    drawMainMenu();
    break;
  case Screen::broadcast:
    drawBroadcastMenu();
    break;
  case Screen::graphicPage:
  case Screen::segmentPage:
    drawPageEditor();
    break;
  case Screen::diagnostics:
    drawDiagnostics();
    break;
  case Screen::sending:
    drawnProgress_ = 255;
    progressDrawnAt_ = 0;
    drawSending();
    break;
  case Screen::result:
    drawResult();
    break;
  case Screen::target:
    drawTargetMenu();
    break;
  case Screen::targetBarcode:
    drawTargetInput();
    break;
  case Screen::targetProfileKind:
    drawTargetProfileKind();
    break;
  case Screen::targetProfileSize:
    drawTargetProfileSize();
    break;
  case Screen::targetProfileWidth:
    drawTargetProfileDimension(true);
    break;
  case Screen::targetProfileHeight:
    drawTargetProfileDimension(false);
    break;
  case Screen::targetProfileOrientation:
    drawTargetProfileOrientation();
    break;
  case Screen::targetProfileColor:
    drawTargetProfileColor();
    break;
  case Screen::targetActions:
    drawTargetActions();
    break;
  case Screen::targetDetails:
    drawTargetDetails();
    break;
  case Screen::targetRemove:
    drawTargetRemove();
    break;
  case Screen::targetText:
    drawTargetText();
    break;
  case Screen::targetName:
    drawTargetName();
    break;
  case Screen::targetTextStyle:
    drawTargetTextStyle();
    break;
  case Screen::targetImage:
    drawTargetImage();
    break;
  case Screen::targetImageSend:
    drawTargetImageSend();
    break;
  case Screen::targetBlink:
    drawTargetBlink();
    break;
  case Screen::webTransport:
    canvas_.fillScreen(theme::background);
    drawHeader("Browser connection");
    drawMenuLine('1', "USB serial", 45);
    drawMenuLine('2', "Bluetooth", 70);
    drawFooter("Bksp Back");
    present();
    break;
  case Screen::webUi:
    drawWebUi();
    break;
  case Screen::oepl:
    // This mode owns the shared canvas/SD until its network worker stops.
    imageData_ = {};
    bleBridge_.clearStaged();
    oepl_.begin();
    break;
  }
}

void App::handleMainMenu() {
  if (M5Cardputer.Keyboard.isKeyPressed('2')) { consoleRequested_ = true; return; }
  for (const auto& item : menuItems_) {
    if (M5Cardputer.Keyboard.isKeyPressed(item.key)) {
      open(item.destination);
      return;
    }
  }
}

void App::handleBroadcastMenu() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::mainMenu);
    return;
  }

  for (const auto& item : broadcastItems_) {
    if (!M5Cardputer.Keyboard.isKeyPressed(item.key)) {
      continue;
    }

    if (item.destination == Screen::graphicPage) {
      action_ = broadcast::Action::graphicPage;
    } else if (item.destination == Screen::segmentPage) {
      action_ = broadcast::Action::segmentPage;
    } else {
      action_ = broadcast::Action::diagnostics;
    }
    open(item.destination);
    return;
  }
}

void App::handlePageEditor() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::broadcast);
    return;
  }
  if (keys.tab) {
    if (action_ == broadcast::Action::graphicPage) {
      graphicDuration_ = broadcast::next(graphicDuration_);
    } else {
      segmentDuration_ = broadcast::next(segmentDuration_);
    }
    drawPageEditor();
    return;
  }
  if (keys.enter) {
    startBroadcast();
    return;
  }

  for (std::uint8_t candidate = 0; candidate < 8; ++candidate) {
    if (M5Cardputer.Keyboard.isKeyPressed(static_cast<char>('0' + candidate))) {
      page_ = candidate;
      drawPageEditor();
      return;
    }
  }
}

void App::handleDiagnostics() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::broadcast);
  } else if (keys.enter) {
    startBroadcast();
  }
}

void App::handleSending() {
  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed() &&
      M5Cardputer.Keyboard.keysState().backspace) {
    transmitter_.cancel();
  }

  const auto status = transmitter_.status();
  if (status != InfraredTransmitter::Status::sending) {
    result_ = status;
    open(Screen::result);
    return;
  }

  const std::uint8_t progress = transmitter_.progress();
  const std::uint32_t now = millis();
  if (progress != drawnProgress_ && now - progressDrawnAt_ >= 50) {
    drawnProgress_ = progress;
    progressDrawnAt_ = now;
    drawSending();
  }
}

void App::handleResult() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(transmission_ == Transmission::broadcast ? Screen::broadcast : Screen::targetActions);
  } else if (keys.enter) {
    if (transmission_ == Transmission::broadcast) {
      startBroadcast();
    } else if (transmission_ == Transmission::targetBlink) {
      startTargetBlink();
    } else {
      startTargetImage(transmission_);
    }
  }
}

void App::handleTargetMenu() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::mainMenu);
    return;
  }
  const std::size_t pageCount =
      targets_.size() == 0U ? 1U : (targets_.size() + targetRowsPerPage - 1U) / targetRowsPerPage;
  if (keys.tab && pageCount > 1U) {
    targetListPage_ = (targetListPage_ + 1U) % pageCount;
    drawTargetMenu();
    return;
  }
  if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    editingProfile_ = false;
    saveError_ = false;
    resetInput();
    open(Screen::targetBarcode);
    return;
  }
  for (std::size_t row = 0; row < targetRowsPerPage; ++row) {
    const char key = static_cast<char>('2' + row);
    if (!M5Cardputer.Keyboard.isKeyPressed(key)) {
      continue;
    }
    const std::size_t index = targetListPage_ * targetRowsPerPage + row;
    if (index < targets_.size()) {
      selectedTarget_ = static_cast<int>(index);
      const target::Record* record = selectedTarget();
      targetPage_ =
          record == nullptr ? 1U : static_cast<std::uint8_t>(record->profile.imagePage & 7U);
      open(Screen::targetActions);
    }
    return;
  }
}

void App::handleTargetInput() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    saveError_ = false;
    inputError_ = target::ParseError::none;
    if (inputLength_ > 0U) {
      input_[--inputLength_] = '\0';
      drawTargetInput();
    } else {
      open(Screen::target);
    }
    return;
  }
  if (keys.enter) {
    const auto parsed = target::parseBarcode(input_.data());
    if (!parsed.ok()) {
      inputError_ = parsed.error;
      drawTargetInput();
      return;
    }
    pendingTarget_ = editing::preserveSavedRecord(targets_, parsed.record);
    if (pendingTarget_.profile.kind != target::Kind::unknown) {
      if (!savePendingTarget()) {
        drawTargetInput();
      }
    } else {
      editingProfile_ = false;
      saveError_ = false;
      open(Screen::targetProfileKind);
    }
    return;
  }

  constexpr std::size_t limit = 17U;
  for (char character : keys.word) {
    if (inputLength_ >= limit) {
      break;
    }
    if (character >= 'a' && character <= 'z') {
      character = static_cast<char>(character - 'a' + 'A');
    }
    const bool validBarcodeCharacter =
        (inputLength_ == 0U &&
         ((character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9'))) ||
        (inputLength_ > 0U && character >= '0' && character <= '9');
    if (validBarcodeCharacter) {
      input_[inputLength_++] = character;
      input_[inputLength_] = '\0';
      inputError_ = target::ParseError::none;
      saveError_ = false;
    }
  }
  drawTargetInput();
}

void App::handleTargetProfileKind() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(editingProfile_ ? Screen::targetDetails : Screen::targetBarcode);
  } else if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    saveError_ = false;
    pendingTarget_.profileOverridden = true;
    pendingTarget_.profile.kind = target::Kind::graphic;
    pendingTarget_.profile.pp16 = true;
    pendingTarget_.profile.imagePage = 1;
    customProfileSize_ = false;
    profileSizePage_ = 0;
    open(Screen::targetProfileSize);
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    saveError_ = false;
    editing::configureSegment(pendingTarget_);
    if (!savePendingTarget()) {
      drawTargetProfileKind();
    }
  }
}

void App::handleTargetProfileSize() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::targetProfileKind);
    return;
  }
  const std::size_t pageCount = (profileSizeChoices + sizeRowsPerPage - 1U) / sizeRowsPerPage;
  if (keys.tab) {
    profileSizePage_ = (profileSizePage_ + 1U) % pageCount;
    drawTargetProfileSize();
    return;
  }
  for (std::size_t row = 0; row < sizeRowsPerPage; ++row) {
    const char key = static_cast<char>('1' + row);
    if (!M5Cardputer.Keyboard.isKeyPressed(key)) {
      continue;
    }
    const std::size_t index = profileSizePage_ * sizeRowsPerPage + row;
    if (index < sizePresets.size()) {
      customProfileSize_ = false;
      pendingTarget_.profile.width = sizePresets[index].width;
      pendingTarget_.profile.height = sizePresets[index].height;
      pendingTarget_.profile.rotateClockwise = sizePresets[index].rotateClockwise;
      open(Screen::targetProfileColor);
    } else if (index == sizePresets.size()) {
      customProfileSize_ = true;
      dimensionError_ = DimensionError::none;
      resetInput();
      open(Screen::targetProfileWidth);
    }
    return;
  }
}

void App::handleTargetProfileDimension(const bool width) {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    dimensionError_ = DimensionError::none;
    if (inputLength_ > 0U) {
      input_[--inputLength_] = '\0';
      drawTargetProfileDimension(width);
    } else {
      open(width ? Screen::targetProfileSize : Screen::targetProfileWidth);
    }
    return;
  }
  if (keys.enter) {
    std::uint32_t value = 0;
    for (std::size_t index = 0; index < inputLength_; ++index) {
      value = value * 10U + static_cast<std::uint32_t>(input_[index] - '0');
    }
    if (value < minimumCustomDimension || value > maximumCustomDimension) {
      dimensionError_ = DimensionError::range;
      drawTargetProfileDimension(width);
      return;
    }
    if (width) {
      customWidth_ = static_cast<std::uint16_t>(value);
      dimensionError_ = DimensionError::none;
      resetInput();
      open(Screen::targetProfileHeight);
      return;
    }
    customHeight_ = static_cast<std::uint16_t>(value);
    const std::uint32_t pixels = static_cast<std::uint32_t>(customWidth_) * customHeight_;
    if ((pixels & 7U) != 0U) {
      dimensionError_ = DimensionError::alignment;
      drawTargetProfileDimension(false);
      return;
    }
    if (pixels > maximumCustomPixels) {
      dimensionError_ = DimensionError::area;
      drawTargetProfileDimension(false);
      return;
    }
    dimensionError_ = DimensionError::none;
    open(Screen::targetProfileOrientation);
    return;
  }

  for (const char character : keys.word) {
    if (inputLength_ >= 4U || character < '0' || character > '9') {
      continue;
    }
    input_[inputLength_++] = character;
    input_[inputLength_] = '\0';
    dimensionError_ = DimensionError::none;
  }
  drawTargetProfileDimension(width);
}

void App::handleTargetProfileOrientation() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    resetInput();
    open(Screen::targetProfileHeight);
    return;
  }
  if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    pendingTarget_.profile.width = customWidth_;
    pendingTarget_.profile.height = customHeight_;
    pendingTarget_.profile.rotateClockwise = false;
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    pendingTarget_.profile.width = customHeight_;
    pendingTarget_.profile.height = customWidth_;
    pendingTarget_.profile.rotateClockwise = true;
  } else {
    return;
  }
  open(Screen::targetProfileColor);
}

void App::handleTargetProfileColor() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(customProfileSize_ ? Screen::targetProfileOrientation : Screen::targetProfileSize);
    return;
  }
  if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    pendingTarget_.profile.color = target::Color::mono;
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    pendingTarget_.profile.color = target::Color::red;
  } else if (M5Cardputer.Keyboard.isKeyPressed('3')) {
    pendingTarget_.profile.color = target::Color::yellow;
  } else if (M5Cardputer.Keyboard.isKeyPressed('4')) {
    pendingTarget_.profile.color = target::Color::fourColor;
  } else {
    return;
  }
  saveError_ = false;
  if (!savePendingTarget()) {
    drawTargetProfileColor();
  }
}

void App::handleTargetActions() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    open(Screen::target);
    return;
  }
  if (keys.backspace) {
    open(Screen::target);
    return;
  }
  if (keys.tab && record->profile.kind == target::Kind::graphic) {
    transmitter_.setFast(!transmitter_.fast());
    drawTargetActions();
    return;
  }
  if (M5Cardputer.Keyboard.isKeyPressed('n')) {
    resetInput();
    std::snprintf(input_.data(), input_.size(), "%s", record->name.data());
    inputLength_ = std::strlen(input_.data());
    open(Screen::targetName);
    return;
  }
  if (record->profile.kind == target::Kind::graphic) {
    if (M5Cardputer.Keyboard.isKeyPressed('1')) {
      resetInput();
      composeError_ = false;
      open(Screen::targetText);
    } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
      imageListPage_ = 0;
      imageComposeError_ = false;
      images_.refresh();
      open(Screen::targetImage);
    } else if (M5Cardputer.Keyboard.isKeyPressed('3')) {
      open(Screen::targetBlink);
    } else if (M5Cardputer.Keyboard.isKeyPressed('4')) {
      open(Screen::targetDetails);
    }
  } else if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    open(Screen::targetDetails);
  }
}

void App::handleTargetText() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    composeError_ = false;
    if (inputLength_ > 0U) {
      input_[--inputLength_] = '\0';
      drawTargetText();
    } else {
      open(Screen::targetActions);
    }
    return;
  }
  if (keys.tab) {
    textStylePage_ = 0;
    open(Screen::targetTextStyle);
    return;
  }
  if (keys.enter) {
    if (inputLength_ == 0U) {
      return;
    }
    if (prepareTextImage()) {
      startTargetImage(Transmission::targetText);
    } else {
      composeError_ = true;
      drawTargetText();
    }
    return;
  }
  for (const char character : keys.word) {
    if (inputLength_ >= input_.size() - 1U || character < 32 || character > 126) {
      continue;
    }
    input_[inputLength_++] = character;
    input_[inputLength_] = '\0';
    composeError_ = false;
  }
  drawTargetText();
}

void App::handleTargetTextStyle() {
  auto& keyboard = M5Cardputer.Keyboard;
  const auto& keys = keyboard.keysState();
  if (keys.backspace || keys.enter) {
    composeError_ = false;
    open(Screen::targetText);
    return;
  }
  if (keys.tab) {
    textStylePage_ ^= 1U;
    drawTargetTextStyle();
    return;
  }

  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    open(Screen::target);
    return;
  }
  if (textStylePage_ == 0U) {
    if (keyboard.isKeyPressed('1')) {
      textStyle_.font =
          static_cast<render::TextFont>((static_cast<std::uint8_t>(textStyle_.font) + 1U) % 5U);
    } else if (keyboard.isKeyPressed('2')) {
      textStyle_.size =
          static_cast<render::TextSize>((static_cast<std::uint8_t>(textStyle_.size) + 1U) % 4U);
    } else if (keyboard.isKeyPressed('3')) {
      textStyle_.align =
          static_cast<render::TextAlign>((static_cast<std::uint8_t>(textStyle_.align) + 1U) % 3U);
    } else if (keyboard.isKeyPressed('4')) {
      textStyle_.foreground =
          nextTextColor(record->profile.color, textStyle_.foreground, textStyle_.background);
    } else {
      return;
    }
  } else {
    if (keyboard.isKeyPressed('1')) {
      textStyle_.background =
          nextTextColor(record->profile.color, textStyle_.background, textStyle_.foreground);
    } else if (keyboard.isKeyPressed('2')) {
      textStyle_.vertical = static_cast<render::TextVertical>(
          (static_cast<std::uint8_t>(textStyle_.vertical) + 1U) % 3U);
    } else if (keyboard.isKeyPressed('3')) {
      targetPage_ = static_cast<std::uint8_t>((targetPage_ + 1U) & 7U);
    } else if (keyboard.isKeyPressed('4')) {
      textStyle_ = {};
      targetPage_ = static_cast<std::uint8_t>(record->profile.imagePage & 7U);
    } else {
      return;
    }
  }
  composeError_ = false;
  drawTargetTextStyle();
}

void App::handleTargetImage() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::targetActions);
    return;
  }
  if (images_.status() != ImageLibrary::Status::ready || images_.size() == 0U) {
    if (keys.enter) {
      imageComposeError_ = false;
      images_.refresh();
      drawTargetImage();
    }
    return;
  }

  const std::size_t pageCount = (images_.size() + imageRowsPerPage - 1U) / imageRowsPerPage;
  if (keys.tab && pageCount > 1U) {
    imageListPage_ = (imageListPage_ + 1U) % pageCount;
    imageComposeError_ = false;
    drawTargetImage();
    return;
  }
  for (std::size_t row = 0; row < imageRowsPerPage; ++row) {
    if (!M5Cardputer.Keyboard.isKeyPressed(static_cast<char>('1' + row))) {
      continue;
    }
    const std::size_t index = imageListPage_ * imageRowsPerPage + row;
    const char* path = images_.path(index);
    if (path != nullptr && prepareFileImage(path)) {
      selectedImage_ = index;
      open(Screen::targetImageSend);
    } else {
      imageComposeError_ = true;
      drawTargetImage();
    }
    return;
  }
}

void App::handleTargetImageSend() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::targetImage);
  } else if (keys.tab) {
    targetPage_ = static_cast<std::uint8_t>((targetPage_ + 1U) & 7U);
    drawTargetImageSend();
  } else if (keys.enter) {
    startTargetImage(Transmission::targetImage);
  }
}

void App::handleTargetBlink() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace) {
    open(Screen::targetActions);
  } else if (keys.tab) {
    blinkDurationIndex_ = (blinkDurationIndex_ + 1U) % blinkDurations.size();
    drawTargetBlink();
  } else if (keys.enter) {
    startTargetBlink();
  }
}

void App::handleTargetDetails() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    open(Screen::target);
    return;
  }
  if (keys.backspace) {
    open(Screen::targetActions);
  } else if (M5Cardputer.Keyboard.isKeyPressed('1')) {
    pendingTarget_ = *record;
    editingProfile_ = true;
    saveError_ = false;
    open(Screen::targetProfileKind);
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    open(Screen::targetRemove);
  }
}

void App::handleTargetRemove() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.backspace || M5Cardputer.Keyboard.isKeyPressed('1')) {
    open(Screen::targetDetails);
  } else if (M5Cardputer.Keyboard.isKeyPressed('2')) {
    if (selectedTarget_ >= 0) {
      targets_.erase(static_cast<std::size_t>(selectedTarget_));
    }
    selectedTarget_ = -1;
    targetListPage_ = 0;
    open(Screen::target);
  }
}

void App::handleWebUi() {
  const auto status = transmitter_.status();
  const bool isSending = (status == InfraredTransmitter::Status::sending);

  if (M5Cardputer.Keyboard.isChange() && M5Cardputer.Keyboard.isPressed()) {
    const auto& keys = M5Cardputer.Keyboard.keysState();
    if (keys.tab && !isSending) {
      transmitter_.setFast(!transmitter_.fast());
      drawWebUi();
      return;
    }
    if (keys.backspace) {
      if (isSending) {
        transmitter_.cancel();
        return;
      } else if (bleBridge_.hasStaged()) {
        bleBridge_.clearStaged();
        drawnProgress_ = 255;
        drawWebUi();
        return;
      } else {
        bleBridge_.stop();
        open(Screen::mainMenu);
        return;
      }
    } else if (keys.enter) {
      if (!isSending && bleBridge_.hasStaged()) {
        if (bleBridge_.transmitStaged()) {
          drawnProgress_ = 255;
          drawWebUi();
        }
        return;
      }
    }
  }

  const std::uint8_t prog = transmitter_.progress();
  const std::uint32_t now = millis();
  if (isSending) {
    if ((prog != drawnProgress_ && now - progressDrawnAt_ >= 30) ||
        (now - progressDrawnAt_ >= 100)) {
      drawnProgress_ = prog;
      progressDrawnAt_ = now;
      drawWebUi();
    }
  } else {
    if (now - progressDrawnAt_ >= 300) {
      progressDrawnAt_ = now;
      drawWebUi();
    }
  }
}

void App::startBroadcast() {
  broadcast::Frame frame{};
  switch (action_) {
  case broadcast::Action::graphicPage:
    frame = broadcast::makeGraphicPageFrame(page_, graphicDuration_);
    break;
  case broadcast::Action::segmentPage:
    frame = broadcast::makeSegmentPageFrame(page_, segmentDuration_);
    break;
  case broadcast::Action::diagnostics:
    frame = broadcast::makeDiagnosticsFrame();
    break;
  }
  transmission_ = Transmission::broadcast;
  if (transmitter_.start(frame)) {
    open(Screen::sending);
    return;
  }

  result_ = InfraredTransmitter::Status::error;
  open(Screen::result);
}

void App::startTargetBlink() {
  const target::Record* record = selectedTarget();
  if (record == nullptr || record->profile.kind != target::Kind::graphic) {
    return;
  }
  InfraredTransmitter::Plan plan{};
  plan.steps[0] = {
      target::makeBlinkFrame(record->wirePlid, selectedBlinkDuration(), esl::Encoding::pp4), 200,
      1};
  plan.size = 1;
  transmission_ = Transmission::targetBlink;
  if (transmitter_.start(plan)) {
    open(Screen::sending);
    return;
  }
  result_ = InfraredTransmitter::Status::error;
  open(Screen::result);
}

void App::startTargetImage(const Transmission content) {
  const target::Record* record = selectedTarget();
  if (record == nullptr || imageData_.bytes.empty()) {
    composeError_ = true;
    open(Screen::targetText);
    return;
  }
  const InfraredTransmitter::ImageTransfer transfer{
      record->wirePlid,
      imageData_.bytes.data(),
      imageData_.bytes.size(),
      record->profile.width,
      record->profile.height,
      imageData_.compression,
      targetPage_,
      record->profile.pp16 ? esl::Encoding::pp16 : esl::Encoding::pp4,
  };
  transmission_ = content;
  if (transmitter_.startImage(transfer)) {
    open(Screen::sending);
    return;
  }
  result_ = InfraredTransmitter::Status::error;
  open(Screen::result);
}

bool App::prepareTextImage() {
  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    return false;
  }

  canvas_.fillScreen(theme::background);
  drawHeader("Preparing text");
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Preparing your text...", screenWidth / 2, 68);
  present();

  return render::text(M5Cardputer.Display, record->profile, input_.data(), textStyle_, imageData_);
}

bool App::prepareFileImage(const char* path) {
  const target::Record* record = selectedTarget();
  if (record == nullptr || path == nullptr) {
    return false;
  }

  canvas_.fillScreen(theme::background);
  drawHeader("Preparing image");
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Preparing your image...", screenWidth / 2, 68);
  present();

  return render::file(M5Cardputer.Display, SD, path, record->profile, imageData_);
}

bool App::savePendingTarget() {
  const bool namingNewTarget = !editingProfile_ && pendingTarget_.name[0] == '\0';
  const int index = targets_.upsert(pendingTarget_);
  if (index < 0) {
    saveError_ = true;
    return false;
  }
  saveError_ = false;
  editingProfile_ = false;
  selectedTarget_ = index;
  targetPage_ = static_cast<std::uint8_t>(pendingTarget_.profile.imagePage & 7U);
  if (namingNewTarget) {
    resetInput();
    open(Screen::targetName);
  } else {
    open(Screen::targetActions);
  }
  return true;
}

void App::resetInput() {
  input_.fill('\0');
  inputLength_ = 0;
  inputError_ = target::ParseError::none;
}

void App::drawSplash() {
  canvas_.fillScreen(theme::background);

  canvas_.setFont(&fonts::Font4);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("etag", 108, 49);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::accent, theme::background);
  canvas_.drawString("ADV", 171, 50);

  canvas_.drawFastHLine(45, 68, 150, theme::muted);

  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Electronic tag workbench", screenWidth / 2, 87);

  present();
}

void App::drawMainMenu() {
  canvas_.fillScreen(theme::background);
  drawHeader("etag / Cardputer ADV");

  drawMenuItem(menuItems_[0], 35);
  drawMenuLine('2', "Wired diagnostics", 55);
  drawMenuItem(menuItems_[1], 75);
  drawMenuItem(menuItems_[2], 95);
  drawFooter("1-4 Select");

  present();
}

void App::drawBroadcastMenu() {
  canvas_.fillScreen(theme::background);
  drawHeader("Broadcast");

  drawMenuItem(broadcastItems_[0], 48);
  drawMenuItem(broadcastItems_[1], 73);
  drawMenuItem(broadcastItems_[2], 98);
  drawFooter("Bksp  Back");

  present();
}

void App::drawPageEditor() {
  canvas_.fillScreen(theme::background);
  drawHeader(broadcast::actionLabel(action_));

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Page", 18, 54);
  canvas_.drawString("Show for", 18, 86);

  char pageText[4]{'[', static_cast<char>('0' + page_), ']', '\0'};
  canvas_.setFont(&fonts::Font4);
  canvas_.setTextDatum(middle_right);
  canvas_.setTextColor(theme::accent, theme::background);
  canvas_.drawString(pageText, screenWidth - 18, 54);

  canvas_.setFont(&fonts::Font2);
  canvas_.drawString(selectedDurationLabel(), screenWidth - 18, 86);

  drawFooter("0-7 Page / Tab Time", 106);
  drawFooter("Enter Send", 124);

  present();
}

void App::drawDiagnostics() {
  canvas_.fillScreen(theme::background);
  drawHeader("Diagnostics");

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Graphic tag status screen", screenWidth / 2, 57);

  canvas_.setFont(&fonts::Font2);
  canvas_.drawString("Visible for 10 seconds", screenWidth / 2, 78);
  drawFooter("Enter Send", 111);
  drawFooter("Bksp Back", 126);

  present();
}

void App::drawSending() {
  canvas_.fillScreen(theme::background);
  drawHeader(transmission_ == Transmission::broadcast ? "Broadcasting" : "Sending to target");

  const std::uint8_t progress = transmitter_.progress();
  char numBuf[8]{};
  std::snprintf(numBuf, sizeof(numBuf), "%u", progress);

  canvas_.setFont(&fonts::Font6);
  const int numW = canvas_.textWidth(numBuf);
  canvas_.setFont(&fonts::Font4);
  const int pctW = canvas_.textWidth("%");
  const int totalW = numW + pctW + 4;
  const int startX = (screenWidth - totalW) / 2;

  canvas_.setTextDatum(top_left);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.setFont(&fonts::Font6);
  canvas_.drawString(numBuf, startX, 22);
  canvas_.setFont(&fonts::Font4);
  canvas_.drawString("%", startX + numW + 4, 38);

  constexpr std::int32_t barX = 16;
  constexpr std::int32_t barY = 74;
  constexpr std::int32_t barW = 208;
  constexpr std::int32_t barH = 14;

  canvas_.drawRect(barX, barY, barW, barH, theme::text);
  const std::int32_t fillW = (static_cast<std::int32_t>(progress) * (barW - 4)) / 100;
  if (fillW > 0) {
    canvas_.fillRect(barX + 2, barY + 2, fillW, barH - 4, theme::accent);
  }

  char summary[48]{};
  formatAction(summary, sizeof(summary));
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::muted, theme::background);
  drawFitted(summary, screenWidth / 2, 102, 212);

  drawFooter("Bksp Stop", 124);

  present();
}

void App::drawResult() {
  canvas_.fillScreen(theme::background);

  const char* title = "Transmit error";
  if (result_ == InfraredTransmitter::Status::succeeded) {
    title = "IR transmitted";
  } else if (result_ == InfraredTransmitter::Status::cancelled) {
    title = "Stopped";
  }
  drawHeader(title);

  char summary[48]{};
  formatAction(summary, sizeof(summary));
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  drawFitted(summary, screenWidth / 2, 60, 212);

  drawFooter("Check tag; no acknowledgement", 83);
  drawFooter("Enter Repeat", 103);
  drawFooter(transmission_ == Transmission::broadcast ? "Bksp Back" : "Bksp Target", 123);

  present();
}

void App::drawTargetMenu() {
  canvas_.fillScreen(theme::background);
  drawHeader(targets_.healthy() ? "IR devices" : "Storage error - retry save");

  drawMenuLine('1', "Add target", 43);
  for (std::size_t row = 0; row < targetRowsPerPage; ++row) {
    const std::size_t index = targetListPage_ * targetRowsPerPage + row;
    const target::Record* record = targets_.get(index);
    if (record == nullptr) {
      continue;
    }
    char label[32]{};
    formatTargetLabel(*record, label, sizeof(label));
    drawMenuLine(static_cast<char>('2' + row), label, static_cast<std::int32_t>(64 + row * 20));
  }

  const std::size_t pageCount =
      targets_.size() == 0U ? 1U : (targets_.size() + targetRowsPerPage - 1U) / targetRowsPerPage;
  drawFooter(pageCount > 1U ? "Tab More  Bksp Back" : "Bksp Back", 126);
  present();
}

void App::drawTargetInput() {
  canvas_.fillScreen(theme::background);
  drawHeader(saveError_ ? "Save failed" : "ESL barcode");

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Enter the tag's barcode", 14, 44);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString(inputLength_ == 0U ? "_" : input_.data(), screenWidth / 2, 68);
  canvas_.fillRect(14, 82, screenWidth - 28, 1, theme::accent);

  char count[12]{};
  std::snprintf(count, sizeof(count), "%u / 17", static_cast<unsigned>(inputLength_));
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_right);
  canvas_.setTextColor(theme::text, theme::background);
  if (inputError_ == target::ParseError::none && !saveError_)
    canvas_.drawString(count, screenWidth - 14, 94);
  if (inputError_ != target::ParseError::none) {
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(theme::accent, theme::background);
    drawFitted(target::parseErrorLabel(inputError_), 14, 94, 212);
  }
  if (saveError_) {
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(theme::accent, theme::background);
    drawFitted("Storage failed or 9 slots full", 14, 94, 212);
  }
  drawFooter(saveError_ ? "Enter Retry / Bksp Back" : "Enter Save / Bksp Del", 123);
  present();
}

void App::drawTargetProfileKind() {
  canvas_.fillScreen(theme::background);
  drawHeader(saveError_ ? "Save failed" : "Choose tag family");

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Choose your display", 18, 43);
  drawMenuLine('1', "Graphic display", 68);
  drawMenuLine('2', "Segment display", 94);
  drawFooter(saveError_ ? "Storage/full / Bksp Back" : "Bksp Back", 124);
  present();
}

void App::drawTargetProfileSize() {
  canvas_.fillScreen(theme::background);
  drawHeader("Native image size");

  for (std::size_t row = 0; row < sizeRowsPerPage; ++row) {
    const std::size_t index = profileSizePage_ * sizeRowsPerPage + row;
    if (index > sizePresets.size()) {
      continue;
    }
    if (index == sizePresets.size()) {
      drawMenuLine(static_cast<char>('1' + row), "Custom size",
                   static_cast<std::int32_t>(42 + row * 21));
      continue;
    }
    drawMenuLine(static_cast<char>('1' + row), sizePresets[index].label,
                 static_cast<std::int32_t>(42 + row * 21));
  }
  drawFooter("Tab More  Bksp Back", 126);
  present();
}

void App::drawTargetProfileDimension(const bool width) {
  canvas_.fillScreen(theme::background);
  drawHeader(width ? "Display width" : "Display height");

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  if (width) {
    canvas_.drawString("Width in pixels", 14, 43);
  } else {
    char knownWidth[30]{};
    std::snprintf(knownWidth, sizeof(knownWidth), "Width: %u px", customWidth_);
    canvas_.drawString(knownWidth, 14, 43);
  }

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.drawString(inputLength_ == 0U ? "_" : input_.data(), screenWidth / 2, 68);
  canvas_.fillRect(14, 82, screenWidth - 28, 1, theme::accent);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  if (dimensionError_ != DimensionError::none) {
    canvas_.setTextColor(theme::accent, theme::background);
    const char* error =
        dimensionError_ == DimensionError::range
            ? "Use 8 to 2048 pixels"
            : (dimensionError_ == DimensionError::alignment ? "Pixel count: multiple of 8"
                                                            : "Image size exceeds limit");
    canvas_.drawString(error, 14, 98);
  }
  drawFooter("Enter Next / Bksp Del", 124);
  present();
}

void App::drawTargetProfileOrientation() {
  canvas_.fillScreen(theme::background);
  drawHeader("Protocol orientation");

  char dimensions[32]{};
  std::snprintf(dimensions, sizeof(dimensions), "Display: %u x %u", customWidth_, customHeight_);
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString(dimensions, 18, 43);
  drawMenuLine('1', "Same as display", 69);
  drawMenuLine('2', "Rotate 90 degrees", 94);
  drawFooter("Bksp Back", 124);
  present();
}

void App::drawTargetProfileColor() {
  canvas_.fillScreen(theme::background);
  drawHeader(saveError_ ? "Save failed - retry color" : "Display colors");
  drawMenuLine('1', "Black + white", 42);
  drawMenuLine('2', "B/W + red", 63);
  drawMenuLine('3', "B/W + yellow", 84);
  drawMenuLine('4', "B/W + red + yellow", 105);
  drawFooter(saveError_ ? "Storage/full / Bksp Back" : "Bksp Back", 127);
  present();
}

void App::drawTargetActions() {
  canvas_.fillScreen(theme::background);
  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    drawHeader("Target");
    drawFooter("Bksp Back", 124);
    present();
    return;
  }

  char title[32]{};
  formatTargetLabel(*record, title, sizeof(title));
  drawHeader(title);
  if (record->profile.kind == target::Kind::graphic) {
    drawMenuLine('1', "Text", 43);
    drawMenuLine('2', "Image", 64);
    drawMenuLine('3', "Blink LED", 85);
    drawMenuLine('4', "Details", 106);
  } else {
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.drawString("Segment profile detected", 18, 52);
    drawMenuLine('1', "Details", 79);
  }
  drawFooter(transmitter_.fast() ? "Tab: Fast / N Name" : "Tab: Reliable / N Name", 127);
  present();
}

void App::drawTargetText() {
  canvas_.fillScreen(theme::background);
  const target::Record* record = selectedTarget();
  char title[32]{"Compose"};
  if (record != nullptr)
    formatTargetLabel(*record, title, sizeof(title));
  drawHeader(title);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  if (inputLength_ == 0U) {
    canvas_.setTextColor(theme::accent, theme::background);
    canvas_.drawString("Start typing...", 22, 65);
  } else {
    // Scroll by display lines to keep the insertion point visible.
    std::array<std::array<char, 65>, 16> lines{};
    std::size_t line = 0;
    std::size_t length = 0;
    for (std::size_t i = 0; i < inputLength_; ++i) {
      lines[line][length] = input_[i];
      lines[line][length + 1] = '\0';
      if (canvas_.textWidth(lines[line].data()) > 208 && length > 0) {
        lines[line][length] = '\0';
        ++line;
        length = 0;
        lines[line][length] = input_[i];
      }
      ++length;
    }
    const std::size_t first = line > 2 ? line - 2 : 0;
    for (std::size_t i = first; i <= line; ++i) {
      canvas_.drawString(lines[i].data(), 14, 44 + (i - first) * 23);
    }
    const int cursorX = 14 + canvas_.textWidth(lines[line].data());
    canvas_.drawFastVLine(cursorX + 1, 36 + (line - first) * 23, 17, theme::accent);
  }

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(composeError_ ? theme::accent : theme::text, theme::background);
  if (composeError_) {
    canvas_.drawString("Too big: Tab > Size", 14, 105);
  } else if (record != nullptr) {
    char count[20]{};
    std::snprintf(count, sizeof(count), "%u / 64", static_cast<unsigned>(inputLength_));
    canvas_.drawString(count, 14, 107);
  }
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.drawString("Tab Style   Enter Send", 120, 124);
  present();
}

void App::drawTargetTextStyle() {
  canvas_.fillScreen(theme::background);
  char heading[20]{};
  std::snprintf(heading, sizeof(heading), "Text style  %u/2", textStylePage_ + 1U);
  drawHeader(heading);

  const char* labels[4]{};
  char pageValue[8]{};
  const char* values[4]{};
  if (textStylePage_ == 0U) {
    labels[0] = "Font";
    labels[1] = "Size";
    labels[2] = "Align";
    labels[3] = "Ink";
    values[0] = fontLabel(textStyle_.font);
    values[1] = sizeLabel(textStyle_.size);
    values[2] = alignLabel(textStyle_.align);
    values[3] = colorLabel(textStyle_.foreground);
  } else {
    labels[0] = "Paper";
    labels[1] = "Position";
    labels[2] = "Page";
    labels[3] = "Reset";
    values[0] = colorLabel(textStyle_.background);
    values[1] = verticalLabel(textStyle_.vertical);
    std::snprintf(pageValue, sizeof(pageValue), "%u", targetPage_);
    values[2] = pageValue;
    values[3] = "Defaults";
  }

  canvas_.setFont(&fonts::Font2);
  for (std::size_t row = 0; row < 4U; ++row) {
    const std::int32_t y = 43 + static_cast<std::int32_t>(row) * 20;
    char key[4]{'[', static_cast<char>('1' + row), ']', '\0'};
    canvas_.setTextDatum(middle_left);
    canvas_.setTextColor(theme::accent, theme::background);
    canvas_.drawString(key, 16, y);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.drawString(labels[row], 48, y);
    canvas_.setTextDatum(middle_right);
    canvas_.drawString(values[row], screenWidth - 16, y);
  }
  canvas_.setTextDatum(middle_center);
  canvas_.drawString("Tab More   Enter Done", 120, 124);
  present();
}

void App::drawTargetImage() {
  canvas_.fillScreen(theme::background);
  drawHeader("Image");

  if (images_.status() == ImageLibrary::Status::noCard) {
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.drawString("Insert a microSD card", screenWidth / 2, 58);
    canvas_.setFont(&fonts::Font2);
    canvas_.drawString("ENTER RETRY", screenWidth / 2, 83);
    drawFooter("Bksp Back", 124);
    present();
    return;
  }
  if (images_.status() == ImageLibrary::Status::error) {
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.drawString("Could not read microSD", screenWidth / 2, 58);
    canvas_.setFont(&fonts::Font2);
    canvas_.drawString("ENTER RETRY", screenWidth / 2, 83);
    drawFooter("Bksp Back", 124);
    present();
    return;
  }
  if (images_.size() == 0U) {
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.drawString("No images found", screenWidth / 2, 55);
    canvas_.setFont(&fonts::Font2);
    canvas_.drawString("/etag/images", screenWidth / 2, 78);
    canvas_.drawString("PNG  JPG  BMP  QOI", screenWidth / 2, 95);
    drawFooter("Enter Scan / Bksp Back", 124);
    present();
    return;
  }

  for (std::size_t row = 0; row < imageRowsPerPage; ++row) {
    const std::size_t index = imageListPage_ * imageRowsPerPage + row;
    const char* name = images_.name(index);
    if (name == nullptr) {
      continue;
    }
    char label[25]{};
    std::strncpy(label, name, sizeof(label) - 1U);
    if (std::strlen(name) >= sizeof(label)) {
      label[sizeof(label) - 2U] = '~';
    }
    drawMenuLine(static_cast<char>('1' + row), label, static_cast<std::int32_t>(42 + row * 21));
  }
  if (imageComposeError_) {
    drawFooter("Failed / Bksp Back", 127);
  } else {
    const std::size_t pageCount = (images_.size() + imageRowsPerPage - 1U) / imageRowsPerPage;
    drawFooter(pageCount > 1U ? "1-4 Pick / Tab More" : "1-4 Pick / Bksp Back", 127);
  }
  present();
}

void App::drawTargetImageSend() {
  canvas_.fillScreen(theme::background);
  drawHeader("Send image");

  const target::Record* record = selectedTarget();
  const char* name = images_.name(selectedImage_);
  char label[31]{"Selected image"};
  if (name != nullptr) {
    std::strncpy(label, name, sizeof(label) - 1U);
    if (std::strlen(name) >= sizeof(label)) {
      label[sizeof(label) - 2U] = '~';
    }
  }

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  drawFitted(label, screenWidth / 2, 52, 212);

  canvas_.setFont(&fonts::Font2);
  char details[40]{};
  if (record != nullptr) {
    std::snprintf(details, sizeof(details), "Page %u", targetPage_);
  }
  canvas_.drawString(details, screenWidth / 2, 79);
  canvas_.fillRect(42, 89, screenWidth - 84, 1, theme::accent);
  drawFooter("Tab Page  Enter Send", 108);
  drawFooter("Bksp Images", 126);
  present();
}

void App::drawTargetBlink() {
  canvas_.fillScreen(theme::background);
  drawHeader("Blink LED");

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Green LED duration", screenWidth / 2, 48);

  canvas_.setFont(&fonts::Font4);
  canvas_.setTextColor(theme::accent, theme::background);
  canvas_.drawString(selectedBlinkDurationLabel(), screenWidth / 2, 73);
  canvas_.fillRect(52, 91, screenWidth - 104, 2, theme::accent);

  drawFooter("Tab Time  Enter Send", 110);
  drawFooter("Bksp Back", 127);
  present();
}

void App::drawTargetDetails() {
  canvas_.fillScreen(theme::background);
  drawHeader("Target details");
  const target::Record* record = selectedTarget();
  if (record == nullptr) {
    present();
    return;
  }

  char line[48]{};
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  drawFitted(target::profileName(record->typeCode), 14, 44, 212);
  if (record->profile.kind == target::Kind::graphic) {
    std::snprintf(line, sizeof(line), "%u x %u / %s", displayWidth(record->profile),
                  displayHeight(record->profile), target::colorLabel(record->profile.color));
    drawFitted(line, 14, 66, 212);
  }
  drawMenuLine('1', "Edit profile", 91);
  drawMenuLine('2', "Remove target", 112);
  present();
}

void App::drawTargetRemove() {
  canvas_.fillScreen(theme::background);
  drawHeader("Remove target?");
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::text, theme::background);
  canvas_.drawString("Delete this saved tag?", screenWidth / 2, 56);
  drawMenuLine('1', "Keep", 82);
  drawMenuLine('2', "Remove", 105);
  drawFooter("Bksp Keep", 128);
  present();
}

void App::drawWebUi() {
  canvas_.fillScreen(theme::background);

  const auto status = transmitter_.status();
  if (status == InfraredTransmitter::Status::sending) {
    drawHeader("IR transmitting");

    const std::uint8_t prog = transmitter_.progress();
    char numBuf[8]{};
    std::snprintf(numBuf, sizeof(numBuf), "%u", prog);

    canvas_.setFont(&fonts::Font6);
    const int numW = canvas_.textWidth(numBuf);
    canvas_.setFont(&fonts::Font4);
    const int pctW = canvas_.textWidth("%");
    const int totalW = numW + pctW + 4;
    const int startX = (screenWidth - totalW) / 2;

    canvas_.setTextDatum(top_left);
    canvas_.setTextColor(theme::text, theme::background);
    canvas_.setFont(&fonts::Font6);
    canvas_.drawString(numBuf, startX, 22);
    canvas_.setFont(&fonts::Font4);
    canvas_.drawString("%", startX + numW + 4, 38);

    constexpr std::int32_t barX = 16;
    constexpr std::int32_t barY = 74;
    constexpr std::int32_t barW = 208;
    constexpr std::int32_t barH = 14;

    canvas_.drawRect(barX, barY, barW, barH, theme::text);
    const std::int32_t fillW = (static_cast<std::int32_t>(prog) * (barW - 4)) / 100;
    if (fillW > 0) {
      canvas_.fillRect(barX + 2, barY + 2, fillW, barH - 4, theme::accent);
    }

    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);
    canvas_.setTextColor(theme::muted, theme::background);
    canvas_.drawString("Streaming frames...", screenWidth / 2, 102);

    drawFooter("Bksp Cancel", 124);
    present();
    return;
  }

  if (bleBridge_.hasStaged()) {
    const auto& staged = bleBridge_.staged();

    if (status == InfraredTransmitter::Status::succeeded) {
      drawHeader("IR transmitted");
    } else if (status == InfraredTransmitter::Status::error) {
      drawHeader("IR Failed");
    } else if (status == InfraredTransmitter::Status::cancelled) {
      drawHeader("IR Cancelled");
    } else {
      drawHeader("Ready for ESL");
    }

    canvas_.setTextDatum(top_left);

    canvas_.setFont(&fonts::Font0);
    canvas_.setTextColor(theme::muted, theme::background);
    canvas_.drawString("TARGET:", 12, 28);

    canvas_.setFont(&fonts::Font2);
    canvas_.setTextColor(theme::text, theme::background);
    const char* nameStr =
        (staged.record.name[0] != '\0')
            ? staged.record.name.data()
            : (staged.record.barcode[0] != '\0' ? staged.record.barcode.data()
                                                : target::profileName(staged.record.typeCode));
    drawFitted(nameStr, 62, 27, 166);

    canvas_.setFont(&fonts::Font0);
    canvas_.setTextColor(theme::muted, theme::background);
    char specBuf[48]{};
    std::snprintf(specBuf, sizeof(specBuf), "%ux%u | Pg %u | %u B", staged.width, staged.height,
                  staged.page, static_cast<unsigned>(staged.data.size()));
    canvas_.drawString(specBuf, 12, 49);

    constexpr std::int32_t btnX = 12;
    constexpr std::int32_t btnY = 68;
    constexpr std::int32_t btnW = 216;
    constexpr std::int32_t btnH = 30;

    canvas_.fillRect(btnX, btnY, btnW, btnH, theme::accent);
    canvas_.setTextColor(theme::background, theme::accent);
    canvas_.setFont(&fonts::Font2);
    canvas_.setTextDatum(middle_center);

    if (status == InfraredTransmitter::Status::succeeded) {
      canvas_.drawString("ENTER: REPEAT SEND", btnX + btnW / 2, btnY + 16);
    } else if (status == InfraredTransmitter::Status::error ||
               status == InfraredTransmitter::Status::cancelled) {
      canvas_.drawString("ENTER: RETRY SEND", btnX + btnW / 2, btnY + 16);
    } else {
      canvas_.drawString("ENTER: SEND TO ESL", btnX + btnW / 2, btnY + 16);
    }

    drawFooter(transmitter_.fast() ? "Tab: Fast / Bksp Clear" : "Tab: Reliable / Bksp Clear", 121);
    present();
    return;
  }

  drawHeader(bleBridge_.active() ? "Browser editor" : "Connection error: Bksp");

  canvas_.setTextDatum(top_left);

  canvas_.setFont(&fonts::Font0);
  canvas_.setTextColor(theme::muted, theme::background);
  canvas_.drawString("RUN make studio ON COMPUTER", 12, 35);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextColor(theme::text, theme::background);
  drawFitted("localhost:8000 (see README)", 12, 47, 216);

  canvas_.setFont(&fonts::Font0);
  canvas_.setTextColor(theme::muted, theme::background);
  canvas_.drawString("DEVICE:", 12, 69);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextColor(theme::muted, theme::background);
  drawFitted(bleBridge_.deviceName().c_str(), 58, 66, 170);

  const bool bleConn = bleBridge_.connected();
  const bool bleSecure = bleBridge_.secure();
  canvas_.setTextColor(theme::accent, theme::background);
  if (useBluetooth_) {
    char passkey[24]{};
    std::snprintf(passkey, sizeof(passkey), "PAIR CODE: %06lu",
                  static_cast<unsigned long>(bleBridge_.pairingPasskey()));
    canvas_.setFont(&fonts::Font0);
    canvas_.drawString(passkey, 12, 84);
    canvas_.setFont(&fonts::Font2);
    canvas_.drawString(bleSecure ? "Encrypted BLE connected"
                                : bleConn ? "Enter code in browser prompt"
                                          : "> Pair + connect BLE",
                       12, 99);
  } else {
    canvas_.setFont(&fonts::Font2);
    canvas_.drawString(bleConn ? "USB serial connected" : "> Connect USB serial", 12, 92);
  }

  drawFooter("Bksp Exit", 121);
  present();
}

void App::drawFitted(const char* text, const std::int32_t x, const std::int32_t y,
                     const std::int32_t width) {
  String value(text);
  if (canvas_.textWidth(value) > width) {
    while (value.length() && canvas_.textWidth(value + "...") > width) {
      value.remove(value.length() - 1);
    }
    value += "...";
  }
  canvas_.drawString(value, x, y);
}

void App::drawHeader(const char* title) {
  canvas_.setFont(&fonts::FreeSansBold9pt7b);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  String heading(title);
  while (heading.length() > 0 && canvas_.textWidth(heading) > screenWidth - 24) {
    heading.remove(heading.length() - 1);
  }
  canvas_.drawString(heading, 12, 15);
  canvas_.drawFastHLine(12, 28, screenWidth - 24, theme::muted);
}

void App::drawMenuItem(const MenuItem& item, const std::int32_t y) {
  drawMenuLine(item.key, item.label, y);
}

void App::drawMenuLine(const char key, const char* label, const std::int32_t y) {
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::accent, theme::background);
  char keyLabel[4]{'[', key, ']', '\0'};
  canvas_.drawString(keyLabel, 12, y);

  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::text, theme::background);
  String fitted(label);
  if (canvas_.textWidth(fitted) > screenWidth - 60) {
    while (fitted.length() > 0 && canvas_.textWidth(fitted + "...") > screenWidth - 60) {
      fitted.remove(fitted.length() - 1);
    }
    fitted += "...";
  }
  canvas_.drawString(fitted, 48, y);
}

void App::drawFooter(const char* text, const std::int32_t y) {
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_center);
  canvas_.setTextColor(theme::muted, theme::background);
  String hint(text);
  while (hint.length() > 0 && canvas_.textWidth(hint) > screenWidth - 12) {
    hint.remove(hint.length() - 1);
  }
  canvas_.drawString(hint, screenWidth / 2, y > 124 ? 124 : y);
}

void App::formatAction(char* destination, const std::size_t capacity) const {
  if (transmission_ != Transmission::broadcast) {
    const target::Record* record = selectedTarget();
    if (record != nullptr)
      formatTargetLabel(*record, destination, capacity);
    else
      std::snprintf(destination, capacity, "Selected tag");
    return;
  }
  if (action_ == broadcast::Action::diagnostics) {
    std::snprintf(destination, capacity, "Diagnostics / 10 sec");
    return;
  }

  std::snprintf(destination, capacity, "%s %u / %s",
                action_ == broadcast::Action::graphicPage ? "Graphic" : "Segment", page_,
                selectedDurationLabel());
}

void App::formatTargetLabel(const target::Record& record, char* destination,
                            const std::size_t capacity) const {
  if (record.name[0] != '\0') {
    std::snprintf(destination, capacity, "%s", record.name.data());
    return;
  }
  char plid[9]{};
  target::formatPlid(record.wirePlid, plid);
  std::snprintf(destination, capacity, "Tag %s", plid + 4);
}

void App::handleTargetName() {
  const auto& keys = M5Cardputer.Keyboard.keysState();
  if (keys.tab) {
    resetInput();
    open(Screen::targetActions);
    return;
  }
  if (keys.enter) {
    const auto* record = selectedTarget();
    if (record != nullptr) {
      auto renamed = *record;
      std::snprintf(renamed.name.data(), renamed.name.size(), "%.24s", input_.data());
      const int saved = targets_.upsert(renamed);
      if (saved < 0) { drawTargetName(); return; }
    }
    resetInput();
    open(Screen::targetActions);
    return;
  }
  if (keys.backspace) {
    if (inputLength_)
      input_[--inputLength_] = '\0';
    else {
      open(Screen::targetActions);
      return;
    }
  } else {
    for (const char character : keys.word) {
      if (character >= 32 && character <= 126 && inputLength_ < 24U) {
        input_[inputLength_++] = character;
        input_[inputLength_] = '\0';
      }
    }
  }
  drawTargetName();
}

void App::drawTargetName() {
  canvas_.fillScreen(theme::background);
  drawHeader(targets_.healthy() ? "Name your tag" : "Storage error - retry save");
  canvas_.setFont(&fonts::Font2);
  canvas_.setTextDatum(middle_left);
  canvas_.setTextColor(theme::accent, theme::background);
  canvas_.drawString("Give it a friendly name", 14, 43);
  canvas_.setTextColor(theme::text, theme::background);
  drawFitted(inputLength_ ? input_.data() : "e.g. Kitchen", 14, 70, 212);
  canvas_.drawFastHLine(14, 85, 212, theme::accent);
  canvas_.setTextDatum(middle_center);
  canvas_.drawString("Tab Cancel   Enter Save", 120, 124);
  present();
}

const char* App::selectedDurationLabel() const {
  return action_ == broadcast::Action::graphicPage ? broadcast::durationLabel(graphicDuration_)
                                                   : broadcast::durationLabel(segmentDuration_);
}

std::uint16_t App::selectedBlinkDuration() const {
  return blinkDurations[blinkDurationIndex_].seconds;
}

const char* App::selectedBlinkDurationLabel() const {
  return blinkDurations[blinkDurationIndex_].label;
}

const target::Record* App::selectedTarget() const {
  return selectedTarget_ < 0 ? nullptr : targets_.get(static_cast<std::size_t>(selectedTarget_));
}

void App::present() { canvas_.pushSprite(0, 0); }

} // namespace tagtinker

#endif // ETAG_CARDPUTER_ADV
