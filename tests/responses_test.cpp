#include "services/ResponsesProtocol.h"

#include <cassert>
#include <iostream>

using namespace partner;

int main() {
  ReminderVariation variation;
  for (const int hour : {0, 9, 12, 23}) {
    JsonDocument payload;
    const auto choice = variation.choose(hour * 1000);
    ResponsesProtocol::build(payload, {1793498400, 2026, 11, 1, hour, 54, 0, true},
                             "configured-model", variation, choice, false);
    const auto input = payload["input"].as<std::string>();
    char expected[16];
    snprintf(expected, sizeof(expected), "%02d:54:00 WIB", hour);
    assert(input.find(expected) != std::string::npos);
    assert(payload["model"] == "configured-model");
    assert(payload["instructions"].as<std::string>().find("bahasa Indonesia") != std::string::npos);
    assert(payload["max_output_tokens"] == 1024);
    assert(payload["stream"] == false);
    assert(payload["temperature"].isNull());
    assert(payload["reasoning"].isNull());
    assert(payload["seed"].isNull());
    assert(payload["top_k"].isNull());
    assert(input.find(ReminderVariation::direction(choice.angle)) != std::string::npos);
  }
  auto choice = variation.choose(0);
  variation.accepted(choice, "pesan pertama");
  for (unsigned i = 0; i < 64; ++i) {
    const auto next = variation.choose(i * 8191);
    assert(next.angle != choice.angle);
    assert(next.temperature >= 0.85f && next.temperature <= 1.25f);
  }
  ReminderVariation custom(1.4f, 1.6f);
  for (unsigned i = 0; i < 128; ++i) {
    const float temperature = custom.choose(i * 8191).temperature;
    assert(temperature >= 1.4f && temperature <= 1.6f);
  }
  choice = variation.choose(4); variation.accepted(choice, "pesan kedua");
  choice = variation.choose(5); variation.accepted(choice, "pesan ketiga");
  choice = variation.choose(6); variation.accepted(choice, "pesan keempat");
  JsonDocument varied;
  SensorSnapshot climate;
  climate.online = climate.hasClimate = true;
  climate.temperature10 = 321;
  climate.humidity10 = 557;
  ResponsesProtocol::build(varied, {1793498400, 2026, 11, 1, 16, 54, 0, true},
                           "gpt-6-luna", variation, variation.choose(1024), true, climate);
  const auto input = varied["input"].as<std::string>();
  assert(input.find("suhu 32.1 C, kelembapan 55.7 persen") != std::string::npos);
  assert(input.find("pesan kedua") != std::string::npos);
  assert(input.find("pesan ketiga") != std::string::npos);
  assert(input.find("pesan keempat") != std::string::npos);
  assert(input.find("pesan pertama") == std::string::npos);
  assert(varied["reasoning"]["effort"] == "none");
  assert(varied["temperature"].as<float>() >= 0.85f);
  assert(varied["temperature"].as<float>() <= 1.25f);
  assert(varied["top_p"].isNull() && varied["top_k"].isNull() && varied["seed"].isNull());
  JsonDocument response;
  assert(!deserializeJson(response, R"({"status":"completed","output":[
    {"type":"reasoning","summary":[]},
    {"type":"message","content":[{"type":"output_text","text":"Sekarang pukul sembilan WIB."}]},
    {"type":"message","content":[{"type":"output_text","text":"Semangat!"}]}
  ]})"));
  std::string text, error;
  assert(ResponsesProtocol::readText(response.as<JsonVariantConst>(), text, error));
  assert(text == "Sekarang pukul sembilan WIB. Semangat!");
  for (const char* body : {
      R"({"status":"incomplete","output":[{"type":"message","content":[{"type":"output_text","text":"unfinished"}]}]})",
      R"({"status":"failed","error":{"message":"unavailable"}})",
      R"({"status":"completed","output":[{"type":"message","content":[{"type":"refusal","refusal":"No"}]}]})",
      R"({"status":"completed","output":[{"type":"message","content":[{"type":"output_text","text":" \n\t"}]}]})",
      R"({"status":"completed","output":[]})", R"({})"}) {
    assert(!deserializeJson(response, body));
    assert(!ResponsesProtocol::readText(response.as<JsonVariantConst>(), text, error));
    assert(text.empty() && !error.empty());
  }
  std::cout << "responses: time prompt fixtures, typed text extraction, incomplete and empty output passed\n";
}
