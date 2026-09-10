#ifdef ETAG_CARDPUTER_ADV
// SPDX-License-Identifier: GPL-3.0-only

#include "InfraredTransmitter.hpp"

#include <etag/display/TargetProtocol.hpp>

#include <etag/display/InfraredWaveform.hpp>

#include <driver/gpio.h>
#include <soc/soc_caps.h>

namespace tagtinker {

bool InfraredTransmitter::begin() {
  rmt_config_t config{};
  config.rmt_mode = RMT_MODE_TX;
  config.channel = channel_;
  config.gpio_num = infraredPin_;
  config.clk_div = 8;
  // A complete maximum-length PP4 or PP16 frame fits without ISR refills.
  config.mem_block_num = 4;
  static_assert(164 + 1 <= 4 * SOC_RMT_MEM_WORDS_PER_CHANNEL);
  config.flags = 0;
  config.tx_config.carrier_en = true;
  config.tx_config.carrier_freq_hz = carrierHz_;
  config.tx_config.carrier_duty_percent = 50;
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_HIGH;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
  config.tx_config.idle_output_en = true;
  config.tx_config.loop_en = false;

  if (rmt_config(&config) != ESP_OK ||
      gpio_set_drive_capability(infraredPin_, GPIO_DRIVE_CAP_3) != ESP_OK ||
      rmt_driver_install(channel_, 0, 0) != ESP_OK) {
    status_.store(Status::unavailable);
    return false;
  }

  status_.store(Status::idle);
  return true;
}

bool InfraredTransmitter::start(const esl::Frame& frame) {
  Plan plan{};
  plan.steps[0] = {frame, 200, 15};
  plan.size = 1;
  return start(plan);
}

bool InfraredTransmitter::start(const Plan& plan) {
  if (plan.size == 0U || plan.size > plan.steps.size()) {
    return false;
  }
  for (std::size_t index = 0; index < plan.size; ++index) {
    if (plan.steps[index].frame.size == 0U ||
        plan.steps[index].frame.size > plan.steps[index].frame.bytes.size() ||
        plan.steps[index].repeats == 0U) {
      return false;
    }
  }
  if (!claim())
    return false;
  plan_ = plan;
  work_ = Work::plan;
  return launch();
}

bool InfraredTransmitter::startImage(const ImageTransfer& transfer) {
  if (transfer.data == nullptr || transfer.dataSize == 0U || transfer.dataSize % 20U != 0U ||
      transfer.dataSize > 0xffffU || transfer.width == 0U || transfer.height == 0U ||
      (transfer.compression != 0U && transfer.compression != 2U) || transfer.page > 7U) {
    return false;
  }
  if (!claim())
    return false;
  image_ = transfer;
  imageFast_ = fast_.load();
  work_ = Work::image;
  return launch();
}

bool InfraredTransmitter::claim() {
  auto previous = status_.load();
  do {
    if (previous == Status::unavailable || previous == Status::sending)
      return false;
  } while (!status_.compare_exchange_weak(previous, Status::sending));
  cancelRequested_.store(false);
  progress_.store(0);
  return true;
}

bool InfraredTransmitter::launch() {
  const BaseType_t created = xTaskCreate(transmitTask, "ir-transmit", 4096, this, 1, nullptr);
  if (created != pdPASS) {
    status_.store(Status::error);
    return false;
  }
  return true;
}

void InfraredTransmitter::cancel() {
  if (status_.load() == Status::sending) {
    cancelRequested_.store(true);
  }
}

InfraredTransmitter::Status InfraredTransmitter::status() const { return status_.load(); }

std::uint8_t InfraredTransmitter::progress() const { return progress_.load(); }

bool InfraredTransmitter::encode(const esl::Frame& frame, const std::uint32_t gapUs) {
  infrared::Waveform waveform;
  if (!infrared::encode(frame, gapUs, waveform))
    return false;
  itemCount_ = waveform.size;
  for (std::size_t index = 0; index < itemCount_; ++index) {
    const auto pulse = waveform.pulses[index];
    auto& item = items_[index];
    item.val = 0;
    item.level0 = pulse.burstTicks != 0;
    // RMT treats a zero duration as end-of-transmission, so split quiet-only items.
    item.duration0 = pulse.burstTicks != 0 ? pulse.burstTicks : 1;
    item.level1 = 0;
    item.duration1 = pulse.burstTicks != 0 ? pulse.gapTicks : pulse.gapTicks - 1;
  }
  return true;
}

void InfraredTransmitter::transmitTask(void* context) {
  static_cast<InfraredTransmitter*>(context)->transmit();
}

void InfraredTransmitter::transmit() {
  status_.store(work_ == Work::image ? transmitImage() : transmitPlan());
  vTaskDelete(nullptr);
}

InfraredTransmitter::Status InfraredTransmitter::transmitPlan() {
  std::uint32_t total = 0;
  for (std::size_t index = 0; index < plan_.size; ++index) {
    total += plan_.steps[index].repeats;
  }

  std::uint32_t completed = 0;
  for (std::size_t index = 0; index < plan_.size; ++index) {
    const auto& step = plan_.steps[index];
    if (!send(step.frame, step.repeats, static_cast<std::uint32_t>(step.gapMs) * 1000U, completed,
              total)) {
      return cancelRequested_.load() ? Status::cancelled : Status::error;
    }
  }
  return Status::succeeded;
}

InfraredTransmitter::Status InfraredTransmitter::transmitImage() {
  const bool pp16 = image_.encoding == esl::Encoding::pp16;
  const auto wake = target::makeWakeFrame(image_.wirePlid, image_.encoding);
  const std::uint32_t wakeGapUs = pp16 ? 1000U : 5000U;
  const std::uint32_t wakeRepeats = infrared::wakeRepeats(wake, wakeGapUs);
  const std::uint32_t parameterRepeats = pp16 ? 15U : 2U;
  const std::uint32_t dataRepeats = infrared::dataCopies(image_.encoding, imageFast_);
  const std::uint32_t refreshRepeats = pp16 ? 20U : 2U;
  const std::uint32_t payloadGapUs = pp16 ? 1000U : 5000U;
  const std::uint32_t dataFrames = image_.dataSize / 20U;
  const std::uint32_t total =
      wakeRepeats + parameterRepeats + dataFrames * dataRepeats + refreshRepeats;
  std::uint32_t completed = 0;

  if (!send(wake, wakeRepeats, wakeGapUs, completed, total)) {
    return cancelRequested_.load() ? Status::cancelled : Status::error;
  }
  vTaskDelay(pdMS_TO_TICKS(50));

  if (!send(target::makeImageParametersFrame(
                image_.wirePlid, static_cast<std::uint16_t>(image_.dataSize), image_.compression,
                image_.page, image_.width, image_.height, image_.encoding),
            parameterRepeats, payloadGapUs, completed, total)) {
    return cancelRequested_.load() ? Status::cancelled : Status::error;
  }
  vTaskDelay(pdMS_TO_TICKS(50));

  for (std::uint32_t frameIndex = 0; frameIndex < dataFrames; ++frameIndex) {
    const std::uint8_t* data = image_.data + frameIndex * 20U;
    if (!send(target::makeImageDataFrame(image_.wirePlid, static_cast<std::uint16_t>(frameIndex),
                                         data, image_.encoding),
              dataRepeats, payloadGapUs, completed, total)) {
      return cancelRequested_.load() ? Status::cancelled : Status::error;
    }
  }
  vTaskDelay(pdMS_TO_TICKS(50));

  if (!send(target::makeRefreshFrame(image_.wirePlid, image_.encoding), refreshRepeats, wakeGapUs,
            completed, total)) {
    return cancelRequested_.load() ? Status::cancelled : Status::error;
  }
  return Status::succeeded;
}

bool InfraredTransmitter::send(const esl::Frame& frame, const std::uint32_t repeats,
                               const std::uint32_t gapMicroseconds, std::uint32_t& completed,
                               const std::uint32_t total) {
  const std::uint32_t hardwareGap = gapMicroseconds > 5000U ? 5000U : gapMicroseconds;
  if (!encode(frame, hardwareGap))
    return false;
  for (std::uint32_t repeat = 0; repeat < repeats; ++repeat) {
    if (cancelRequested_.load())
      return false;
    if (rmt_write_items(channel_, items_.data(), itemCount_, false) != ESP_OK)
      return false;
    if (rmt_wait_tx_done(channel_, pdMS_TO_TICKS(100)) != ESP_OK) {
      rmt_tx_stop(channel_);
      return false;
    }
    if (gapMicroseconds > hardwareGap) {
      vTaskDelay(pdMS_TO_TICKS((gapMicroseconds - hardwareGap + 999U) / 1000U));
    }
    ++completed;
    progress_.store(static_cast<std::uint8_t>(completed * 100U / total));
  }
  return !cancelRequested_.load();
}

} // namespace tagtinker

#endif // ETAG_CARDPUTER_ADV
