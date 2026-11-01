#include "I2sAudio.h"
#include "services/PartnerLog.h"
#include <Arduino.h>
#include <app_config.h>
#include <algorithm>
#include <cstring>
#include <driver/pulse_cnt.h>
#include "domain/AudioSamples.h"

namespace partner {
namespace {
int clockEdges(gpio_num_t pin) {
  pcnt_unit_config_t config {};
  config.low_limit = -32768;
  config.high_limit = 32767;
  pcnt_unit_handle_t unit = nullptr;
  pcnt_channel_handle_t channel = nullptr;
  if (pcnt_new_unit(&config, &unit) != ESP_OK) return -1;
  pcnt_chan_config_t input {};
  input.edge_gpio_num = pin;
  input.level_gpio_num = -1;
  int edges = -1;
  if (pcnt_new_channel(unit, &input, &channel) == ESP_OK) {
    pcnt_channel_set_edge_action(channel, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_HOLD);
    pcnt_unit_enable(unit);
    pcnt_unit_clear_count(unit);
    pcnt_unit_start(unit);
    vTaskDelay(pdMS_TO_TICKS(10));
    pcnt_unit_stop(unit);
    pcnt_unit_get_count(unit, &edges);
    pcnt_unit_disable(unit);
    pcnt_del_channel(channel);
  }
  pcnt_del_unit(unit);
  return edges;
}
}  // namespace

bool I2sAudio::begin(std::string& error) {
  if (ready()) return true;
  playback_ = xSemaphoreCreateMutex();
  if (!playback_) { error = "Audio mutex allocation failed"; return false; }
  i2s_chan_config_t channel = I2S_CHANNEL_DEFAULT_CONFIG(I2S_SPEAKER_PORT, I2S_ROLE_MASTER);
  channel.dma_desc_num = 6;
  channel.dma_frame_num = 160;
  channel.auto_clear = true;
  esp_err_t status = i2s_new_channel(&channel, &tx_, &rx_);
  i2s_std_config_t standard {};
  standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config::audioSampleRate);
  standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  standard.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  standard.gpio_cfg.bclk = I2S_SPEAKER_BCLK_PIN;
  standard.gpio_cfg.ws = I2S_SPEAKER_LRC_PIN;
  standard.gpio_cfg.dout = I2S_SPEAKER_DOUT_PIN;
  standard.gpio_cfg.din = MIC_DIN;
  if (status == ESP_OK) status = i2s_channel_init_std_mode(tx_, &standard);
  if (status == ESP_OK) status = i2s_channel_init_std_mode(rx_, &standard);
  if (status == ESP_OK) status = i2s_channel_enable(tx_);
  if (status == ESP_OK) status = i2s_channel_enable(rx_);
  if (status != ESP_OK) {
    if (tx_) { i2s_channel_disable(tx_); i2s_del_channel(tx_); tx_ = nullptr; }
    if (rx_) { i2s_channel_disable(rx_); i2s_del_channel(rx_); rx_ = nullptr; }
    vSemaphoreDelete(playback_);
    playback_ = nullptr;
    error = std::string("I2S duplex init: ") + esp_err_to_name(status);
    return false;
  }
  ready_.store(true);
  partner::log.printf("[audio] duplex %u Hz / 32-bit slots, BCLK=%d WS=%d speaker=%d mic=%d\n",
                config::audioSampleRate, I2S_SPEAKER_BCLK_PIN, I2S_SPEAKER_LRC_PIN,
                I2S_SPEAKER_DOUT_PIN, MIC_DIN);
  const int bclkEdges = clockEdges(I2S_SPEAKER_BCLK_PIN);
  const int wsEdges = clockEdges(I2S_SPEAKER_LRC_PIN);
  partner::log.printf("[audio] clock probe edges/10ms: BCLK=%d WS=%d\n", bclkEdges, wsEdges);
  return true;
}

bool I2sAudio::writePcm(const uint8_t* bytes, size_t size, std::string& error) {
  if (!ready() || size % 2) { error = "Audio unavailable or unaligned PCM"; return false; }
  int32_t stereo[128];
  while (size) {
    const size_t count = std::min(size / 2, size_t(64));
    for (size_t i = 0; i < count; ++i) {
      int16_t sample;
      std::memcpy(&sample, bytes + i * 2, 2);
      stereo[i * 2] = stereo[i * 2 + 1] = pcm16ToI2s32(scalePcm16(sample, config::speakerVolume));
    }
    size_t written = 0;
    const size_t length = count * 8;
    const auto status = i2s_channel_write(tx_, stereo, length, &written, 1000);
    if (status != ESP_OK || written != length) {
      error = std::string("I2S write: ") + esp_err_to_name(status);
      return false;
    }
    bytes += count * 2;
    size -= count * 2;
  }
  return true;
}

bool I2sAudio::readMicrophone(int16_t* samples, size_t count, std::string& error) {
  const auto* start = samples;
  const size_t sampleCount = count;
  int32_t stereo[128];
  uint64_t rawPeak[2] {};
  int peak = 0;
  while (count) {
    const size_t frames = std::min(count, size_t(64));
    size_t read = 0;
    const auto status = i2s_channel_read(rx_, stereo, frames * 8, &read, 1000);
    if (status != ESP_OK || read != frames * 8) {
      error = std::string("I2S microphone: ") + esp_err_to_name(status);
      return false;
    }
    for (size_t i = 0; i < frames; ++i) for (unsigned channel = 0; channel < 2; ++channel) {
      const int64_t value = stereo[i * 2 + channel];
      rawPeak[channel] = std::max<uint64_t>(rawPeak[channel], value < 0 ? -value : value);
    }
    if (!micSelected_) {
      uint64_t energy[2] {};
      for (size_t i = 0; i < frames; ++i) for (unsigned channel = 0; channel < 2; ++channel) {
        const int64_t value = stereo[i * 2 + channel];
        energy[channel] += value < 0 ? -value : value;
      }
      if (energy[0] || energy[1]) {
        micSlot_ = energy[1] > energy[0] ? 1 : 0;
        micSelected_ = true;
        partner::log.printf("[audio] microphone slot=%s level L=%llu R=%llu\n",
                      micSlot_ ? "right" : "left", energy[0], energy[1]);
      }
    }
    for (size_t i = 0; i < frames; ++i) {
      const auto sample = i2s32ToPcm16(stereo[i * 2 + micSlot_], config::microphoneVolume);
      *samples++ = sample;
      peak = std::max(peak, std::abs(static_cast<int>(sample)));
    }
    count -= frames;
  }
  micLevel_.store(static_cast<uint8_t>(std::min(100, peak * 100 / 32768)));
  if (suppressRecognition()) {
    meter_.reset();
    activity_.store(0);
  } else {
    const auto activity = meter_.observe(start, sampleCount, millis());
    activity_.store((static_cast<uint32_t>(activity.peak) << 16) | activity.rms);
  }
  if (millis() - micLoggedAt_ >= 5000) {
    partner::log.printf("[audio] raw microphone peak L=%llu R=%llu\n", rawPeak[0], rawPeak[1]);
    micLoggedAt_ = millis();
  }
  return true;
}

bool I2sAudio::finish(std::string& error) {
  const uint8_t silence[320] {};
  for (unsigned i = 0; i < 6; ++i) if (!writePcm(silence, sizeof(silence), error)) return false;
  vTaskDelay(pdMS_TO_TICKS(60));
  return true;
}

bool I2sAudio::suppressRecognition() const {
  return playing_.load() || millis() - endedAt_.load() < 350;
}

I2sAudio::Playback::Playback(I2sAudio& audio) : audio_(audio) {
  acquired_ = audio_.ready() && xSemaphoreTake(audio_.playback_, portMAX_DELAY) == pdTRUE;
  if (acquired_) audio_.playing_.store(true);
}
I2sAudio::Playback::~Playback() {
  if (!acquired_) return;
  audio_.endedAt_.store(millis());
  audio_.playing_.store(false);
  xSemaphoreGive(audio_.playback_);
}

}  // namespace partner
