#include "I2sAudio.h"
#include "services/PartnerLog.h"
#include <Arduino.h>
#include <app_config.h>
#include <algorithm>
#include <cstring>
#include <driver/pulse_cnt.h>
#include <soc/soc_caps.h>
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
#if MIC_USE_INMP441
  esp_err_t status = i2s_new_channel(&channel, &tx_, &rx_);
#else
  esp_err_t status = i2s_new_channel(&channel, &tx_, nullptr);
#endif
  i2s_std_config_t standard {};
  standard.clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config::audioSampleRate);
  standard.slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO);
  standard.gpio_cfg.mclk = I2S_GPIO_UNUSED;
  standard.gpio_cfg.bclk = I2S_SPEAKER_BCLK_PIN;
  standard.gpio_cfg.ws = I2S_SPEAKER_LRC_PIN;
  standard.gpio_cfg.dout = I2S_SPEAKER_DOUT_PIN;
  standard.gpio_cfg.din = MIC_DIN;
  if (status == ESP_OK) status = i2s_channel_init_std_mode(tx_, &standard);
#if MIC_USE_INMP441
  if (status == ESP_OK) status = i2s_channel_init_std_mode(rx_, &standard);
#endif
  if (status == ESP_OK) status = i2s_channel_enable(tx_);
#if MIC_USE_INMP441
  if (status == ESP_OK) status = i2s_channel_enable(rx_);
#else
  adc_continuous_handle_cfg_t adcHandle {};
  adcHandle.max_store_buf_size = 4096; adcHandle.conv_frame_size = 256;
  if (status == ESP_OK) status = adc_continuous_new_handle(&adcHandle, &adc_);
  adc_digi_pattern_config_t pattern {};
  pattern.atten = ADC_ATTEN_DB_12; pattern.channel = ADC_CHANNEL_0;
  pattern.unit = ADC_UNIT_1; pattern.bit_width = SOC_ADC_DIGI_MAX_BITWIDTH;
  adc_continuous_config_t adcConfig {};
  adcConfig.sample_freq_hz = config::audioSampleRate; adcConfig.conv_mode = ADC_CONV_SINGLE_UNIT_1;
  adcConfig.format = ADC_DIGI_OUTPUT_FORMAT_TYPE2; adcConfig.pattern_num = 1; adcConfig.adc_pattern = &pattern;
  if (status == ESP_OK) status = adc_continuous_config(adc_, &adcConfig);
  if (status == ESP_OK) status = adc_continuous_start(adc_);
#endif
  if (status != ESP_OK) {
#if !MIC_USE_INMP441
    if (adc_) { adc_continuous_deinit(adc_); adc_ = nullptr; }
#endif
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
  return writeWithGain(bytes, size, config::speakerVolume, error);
}

bool I2sAudio::writeUsbPcm(const uint8_t* bytes, size_t size, std::string& error) {
  return writeWithGain(bytes, size, 1.0f, error);
}

bool I2sAudio::writeWithGain(const uint8_t* bytes, size_t size, float gain, std::string& error) {
  if (!ready() || size % 2) { error = "Audio unavailable or unaligned PCM"; return false; }
  int32_t stereo[128];
  while (size) {
    const size_t count = std::min(size / 2, size_t(64));
    for (size_t i = 0; i < count; ++i) {
      int16_t sample;
      std::memcpy(&sample, bytes + i * 2, 2);
      stereo[i * 2] = stereo[i * 2 + 1] = pcm16ToI2s32(scalePcm16(sample, gain));
    }
    const size_t length = count * 8;
    size_t offset = 0;
    while (offset < length) {
      size_t written = 0;
      const auto* data = reinterpret_cast<const uint8_t*>(stereo) + offset;
      const auto status = i2s_channel_write(tx_, data, length - offset, &written, 1000);
      offset += written;
      if (status != ESP_OK || !written) {
        error = std::string("I2S write: ") + esp_err_to_name(status);
        return false;
      }
    }
    bytes += count * 2;
    size -= count * 2;
  }
  return true;
}

bool I2sAudio::readMicrophone(int16_t* samples, size_t count, std::string& error) {
  const auto* start = samples;
  const size_t sampleCount = count;
#if !MIC_USE_INMP441
  size_t copied = 0;
  while (copied < count) {
    if (adcOffset_ == adcCount_) {
      alignas(adc_digi_output_data_t) uint8_t raw[256];
      uint32_t bytes = 0;
      const auto status = adc_continuous_read(adc_, raw, sizeof(raw), &bytes, 1000);
      if (status != ESP_OK) { error = std::string("ADC microphone: ") + esp_err_to_name(status); return false; }
      adcOffset_ = adcCount_ = 0;
      for (uint32_t i = 0; i + SOC_ADC_DIGI_RESULT_BYTES <= bytes; i += SOC_ADC_DIGI_RESULT_BYTES) {
        const auto* value = reinterpret_cast<const adc_digi_output_data_t*>(raw + i);
        if (value->type2.unit != ADC_UNIT_1 || value->type2.channel != ADC_CHANNEL_0) continue;
        const int32_t rawValue = value->type2.data;
        if (dcQ8_ < 0) dcQ8_ = rawValue * 256;
        dcQ8_ += (rawValue * 256 - dcQ8_) / 256;
        adcSamples_[adcCount_++] = clampPcm16((rawValue - dcQ8_ / 256) * 16.f);
      }
    }
    const size_t available = std::min(count - copied, adcCount_ - adcOffset_);
    std::memcpy(samples + copied, adcSamples_ + adcOffset_, available * sizeof(int16_t));
    copied += available;
    adcOffset_ += available;
  }
  int peak = 0;
  for (size_t i = 0; i < sampleCount; ++i) peak = std::max(peak, std::abs(static_cast<int>(start[i])));
  micLevel_.store(static_cast<uint8_t>(std::min(100, peak * 100 / 32768)));
  const auto activity = meter_.observe(start, sampleCount, millis());
  activity_.store((static_cast<uint32_t>(activity.peak) << 16) | activity.rms);
  return true;
#else
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
#endif
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

I2sAudio::Playback::Playback(I2sAudio& audio, TickType_t timeout) : audio_(audio) {
  acquired_ = audio_.ready() && xSemaphoreTake(audio_.playback_, timeout) == pdTRUE;
  if (acquired_) audio_.playing_.store(true);
}
I2sAudio::Playback::~Playback() {
  if (!acquired_) return;
  audio_.endedAt_.store(millis());
  audio_.playing_.store(false);
  xSemaphoreGive(audio_.playback_);
}

}  // namespace partner
