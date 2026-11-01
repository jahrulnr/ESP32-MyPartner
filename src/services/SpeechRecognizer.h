#pragma once

#include <esp_mn_iface.h>
#include <model_path.h>
#include <string>

namespace partner {

// The audio pipeline depends on this contract, not a particular MultiNet version.
class SpeechRecognizer {
 public:
  virtual ~SpeechRecognizer() = default;
  virtual bool begin(srmodel_list_t* models, std::string& error) = 0;
  virtual void end() = 0;
  virtual int sampleCount() const = 0;
  virtual esp_mn_state_t detect(int16_t* samples) = 0;
  virtual const esp_mn_results_t* results() const = 0;
  virtual void reset() = 0;
  virtual const char* name() const = 0;
};

}  // namespace partner
