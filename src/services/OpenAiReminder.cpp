#include "OpenAiReminder.h"
#include "services/PartnerLog.h"
#include "ResponsesProtocol.h"

#include <SpiJsonDocument.h>
#include <app_config.h>
#include <esp_random.h>
#include <cstring>

namespace partner {

bool OpenAiReminder::generate(const LocalTime& now, std::string& text, std::string& error) {
  text.clear();
  if (!config_.apiKey[0] || String(config_.apiKey) == "your-api-key") {
    error = "Configure AI_API_KEY in include/secret.h";
    return false;
  }
  SpiJsonDocument payload;
  const auto choice = variation_.choose(esp_random());
  // These sampling parameters are supported by this OpenAI Responses model
  // with reasoning disabled; leave other configured providers/models untouched.
  const bool sampling = config::reminderRandomSampling &&
                        std::strcmp(config_.baseUrl, "https://api.openai.com/v1") == 0 &&
                        std::strcmp(config_.model, "gpt-6-luna") == 0;
  ResponsesProtocol::build(payload, now, config_.model, variation_, choice, sampling, state_.snapshot().sensor);

  ai::provider::AiHttpRequest request;
  request.url = String(config_.baseUrl) + config_.endpoint;
  request.timeoutMs = config::requestTimeoutMs;
  request.addHeader("Content-Type", "application/json");
  request.addHeader("Authorization", String("Bearer ") + config_.apiKey);
  serializeJson(payload, request.body);

  ai::provider::AiHttpResponse response;
  ai::provider::AiTransportCallbacks callbacks;
  callbacks.userContext = &response;
  callbacks.onDone = [](const ai::provider::AiHttpResponse& completed, void* context) {
    *static_cast<ai::provider::AiHttpResponse*>(context) = completed;
  };
  String submitError;
  if (transport_.executeAsync(request, callbacks, submitError) != ai::provider::AiAsyncSubmitResult::Accepted) {
    error = submitError.c_str();
    return false;
  }
  while (transport_.isBusy()) {
    transport_.poll();
    vTaskDelay(pdMS_TO_TICKS(1));  // Let the idle task run while waiting on the network.
  }
  if (!response.errorMessage.isEmpty()) {
    error = response.errorMessage.c_str();
    return false;
  }
  if (response.statusCode != 200) {
    error = "LLM HTTP " + std::to_string(response.statusCode) + ": " + response.body.c_str();
    return false;
  }
  SpiJsonDocument result;
  const auto parsed = deserializeJson(result, response.body);
  if (parsed) {
    error = std::string("LLM JSON: ") + parsed.c_str();
    return false;
  }
  if (!ResponsesProtocol::readText(result.as<JsonVariantConst>(), text, error)) {
    error += ": " + std::string(response.body.c_str());
    return false;
  }
  variation_.accepted(choice, text);
  partner::log.printf("[llm] %s\n", text.c_str());
  return true;
}

}  // namespace partner
