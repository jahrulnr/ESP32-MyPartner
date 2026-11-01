#pragma once

#include <esp_mn_models.h>
#include "SpeechRecognizer.h"

namespace partner {

class MultiNetRecognizer final : public SpeechRecognizer {
 public:
  bool begin(srmodel_list_t* models, std::string& error) override;
  void end() override;
  int sampleCount() const override;
  esp_mn_state_t detect(int16_t* samples) override;
  const esp_mn_results_t* results() const override;
  void reset() override;
  const char* name() const override { return name_; }

 private:
  const esp_mn_iface_t* model_ = nullptr;
  model_iface_data_t* data_ = nullptr;
  const char* name_ = "uninitialized";
  // The SDK command graph remains valid for the lifetime of the recognizer.
  char hi_[16] {}, hey_[16] {}, hiPhonemes_[16] {}, heyPhonemes_[16] {};
  esp_mn_phrase_t phrases_[2] {};
  esp_mn_node_t nodes_[3] {};
};

}  // namespace partner
