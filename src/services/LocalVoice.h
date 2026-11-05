#pragma once

#include <esp_afe_sr_models.h>
#include <freertos/queue.h>
#include "I2sAudio.h"
#include "BootAudioCapture.h"
#include "MicStream.h"
#include "media/UsbMedia.h"

namespace partner {

class LocalVoice {
 public:
  LocalVoice(I2sAudio& audio, BootAudioCapture& capture, MicStream& stream, UsbMedia& media)
      : audio_(audio), capture_(capture), stream_(stream), media_(media) {}
  void run();
 private:
  bool initialize(std::string& error);
  void cleanup();
  void feed();
  void detect();
  static void feedTask(void* context);

  I2sAudio& audio_;
  BootAudioCapture& capture_;
  MicStream& stream_;
  UsbMedia& media_;
  srmodel_list_t* models_ = nullptr;
  const esp_afe_sr_iface_t* afe_ = nullptr;
  esp_afe_sr_data_t* afeData_ = nullptr;
  int16_t* input_ = nullptr;
  int feedSamples_ = 0;
};

}  // namespace partner
