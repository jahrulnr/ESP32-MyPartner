#include "MultiNetRecognizer.h"

#include <app_config.h>
#include <cstring>
#include "domain/GreetingGate.h"
#ifdef PARTNER_VOICE_REPLAY_PROBE
#include <esp_mn_speech_commands.h>
#include "../../tests/device/VoiceReplayProbe.h"
#endif

namespace partner {
namespace {
bool hasAsset(const srmodel_list_t* models, const char* model, const char* file) {
  if (!models || !models->model_data) return false;
  for (int i = 0; i < models->num; ++i) {
    if (std::strcmp(models->model_name[i], model)) continue;
    const auto* data = models->model_data[i];
    if (!data) return false;
    for (int j = 0; j < data->num; ++j) if (!std::strcmp(data->files[j], file)) return true;
  }
  return false;
}
}  // namespace

bool MultiNetRecognizer::begin(srmodel_list_t* models, std::string& error) {
  char* selected = esp_srmodel_filter(models, ESP_MN_PREFIX, ESP_MN_ENGLISH);
  if (!selected) { error = "No English MultiNet in model partition"; return false; }
  int version = 0;
  const char* stem = nullptr;
  if (!std::strcmp(selected, "mn5q8_en")) { version = 5; stem = "mn5q8"; }
  else if (!std::strcmp(selected, "mn6_en")) { version = 6; stem = "mn6"; }
  else if (!std::strcmp(selected, "mn7_en")) { version = 7; stem = "mn7"; }
  else { error = std::string("Unsupported speech model: ") + selected; return false; }
  for (const auto& file : {std::string(stem) + "_data", std::string(stem) + "_index", std::string("_MODEL_INFO_")}) {
    if (!hasAsset(models, selected, file.c_str())) { error = "Incomplete MultiNet package: missing " + file; return false; }
  }
  if (version != 5 && !hasAsset(models, selected, "vocab")) {
    error = "Incomplete MultiNet package: missing vocab";
    return false;
  }
  if (!hasAsset(models, "fst", "commands_en.txt")) {
    error = "Incomplete MultiNet package: missing fst/commands_en.txt";
    return false;
  }
  name_ = selected;
  model_ = esp_mn_handle_from_name(selected);
  if (!model_ || !(data_ = model_->create(selected, config::voiceCommandWindowMs))) {
    error = "MultiNet init failed";
    return false;
  }

  // MultiNet5 expects phonemes in string; 6 uses words; 7 uses both fields.
  std::strcpy(hi_, version == 5 ? "hi PnRTNk" : "HI PARTNER");
  std::strcpy(hey_, version == 5 ? "hd PnRTNk" : "HEY PARTNER");
  std::strcpy(hiPhonemes_, "hi PnRTNk");
  std::strcpy(heyPhonemes_, "hd PnRTNk");
  phrases_[0] = {}; phrases_[1] = {};
  phrases_[0].string = hi_; phrases_[1].string = hey_;
  phrases_[0].command_id = phrases_[1].command_id = 1;
  if (version == 7) { phrases_[0].phonemes = hiPhonemes_; phrases_[1].phonemes = heyPhonemes_; }
  nodes_[0] = {nullptr, &nodes_[1]};
  nodes_[1] = {&phrases_[0], &nodes_[2]};
  nodes_[2] = {&phrases_[1], nullptr};
  for (int i = 0; i < 2; ++i) {
    const char* input = version == 7 ? phrases_[i].phonemes : phrases_[i].string;
    if (!model_->check_speech_command(data_, input)) { error = "MultiNet rejected greeting phonemes/words"; return false; }
  }
  const auto* invalid = model_->set_speech_commands(data_, &nodes_[0]);
  if (invalid && invalid->num) { error = "MultiNet rejected partner greetings"; return false; }
  model_->set_det_threshold(data_, GreetingGate::minimumConfidence);
  model_->print_active_speech_commands(data_);
#ifdef PARTNER_VOICE_REPLAY_PROBE
  // The existing replay probe compares MN6 grapheme graphs specifically.
  if (version == 6 && esp_mn_commands_alloc(model_, data_) == ESP_OK) {
    replayVoiceProbe(model_, data_);
    model_->set_speech_commands(data_, &nodes_[0]);
    esp_mn_commands_free();
  }
#endif
  return true;
}

void MultiNetRecognizer::end() {
  if (data_) model_->destroy(data_);
  data_ = nullptr;
  model_ = nullptr;
  name_ = "uninitialized";
}
int MultiNetRecognizer::sampleCount() const { return model_->get_samp_chunksize(data_); }
esp_mn_state_t MultiNetRecognizer::detect(int16_t* samples) { return model_->detect(data_, samples); }
const esp_mn_results_t* MultiNetRecognizer::results() const { return model_->get_results(data_); }
void MultiNetRecognizer::reset() { model_->clean(data_); }

}  // namespace partner
