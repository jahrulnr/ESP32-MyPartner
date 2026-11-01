#pragma once

#include <esp_afe_sr_models.h>
#include <freertos/queue.h>
#include "I2sAudio.h"
#include "PicoGreeting.h"
#include "BootAudioCapture.h"
#include "MicStream.h"
#include "SpeechRecognizer.h"
#include "domain/GreetingGate.h"

namespace partner {

class LocalVoice {
 public:
  LocalVoice(I2sAudio& audio, BootAudioCapture& capture, MicStream& stream, SpeechRecognizer& recognizer)
      : audio_(audio), capture_(capture), stream_(stream), recognizer_(recognizer) {}
  void run();
 private:
  bool initialize(std::string& error);
  void cleanup();
  void feed();
  void detect();
  void greet();
  static void feedTask(void* context);
  static void greetingTask(void* context);

  I2sAudio& audio_;
  BootAudioCapture& capture_;
  MicStream& stream_;
  SpeechRecognizer& recognizer_;
  PicoGreeting greeting_;
  srmodel_list_t* models_ = nullptr;
  const esp_afe_sr_iface_t* afe_ = nullptr;
  esp_afe_sr_data_t* afeData_ = nullptr;
  QueueHandle_t greetings_ = nullptr;
  int16_t* input_ = nullptr;
  int feedSamples_ = 0;
};

}  // namespace partner
