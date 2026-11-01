#include "CloudSpeech.h"

#include <HTTPClient.h>
#include <SpiJsonDocument.h>
#include <WiFiClientSecure.h>
#include <app_config.h>
#include "domain/Pcm24To16.h"

namespace partner {
namespace {

// HTTPClient's identity-body loop can otherwise keep waiting on a socket that
// stays connected without sending bytes. Bound the whole speech transaction.
class SpeechClient : public WiFiClientSecure {
 public:
  int available() override { return checkDeadline() ? WiFiClientSecure::available() : 0; }
  uint8_t connected() override { return checkDeadline() ? WiFiClientSecure::connected() : 0; }
  bool timedOut() const { return timedOut_; }

 private:
  bool checkDeadline() {
    constexpr uint32_t limitMs = config::requestTimeoutMs + config::maxSpeechSeconds * 1000;
    if (millis() - startedAt_ >= limitMs) {
      timedOut_ = true;
      stop();
    }
    return !timedOut_;
  }
  uint32_t startedAt_ = millis();
  bool timedOut_ = false;
};

class SpeakerStream : public Stream {
 public:
  SpeakerStream(IPcmSink& sink, IReminderStatus& status)
      : resampler_(sink), assembler_(resampler_), status_(status) {}

  size_t write(const uint8_t* bytes, size_t size) override {
    if (size == 0 || !error_.empty()) return 0;
    constexpr size_t maxBytes = config::ttsSampleRate * 2 * config::maxSpeechSeconds;
    if (size > maxBytes - bytes_) {
      error_ = "TTS exceeded the configured audio duration";
      return 0;
    }
    if (bytes_ == 0) status_.setPhase(ReminderPhase::Speaking);
    if (!assembler_.write(bytes, size, error_)) return 0;
    bytes_ += size;
    return size;
  }
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  int available() override { return 0; }
  int read() override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  bool complete() { return bytes_ > 0 && assembler_.complete() && error_.empty() && resampler_.finish(error_); }
  const std::string& error() const { return error_; }

 private:
  Pcm24To16 resampler_;
  Pcm16Assembler assembler_;
  IReminderStatus& status_;
  size_t bytes_ = 0;
  std::string error_;
};

}  // namespace

bool CloudSpeech::speak(const std::string& text, std::string& error) {
  if (!config_.apiKey[0] || String(config_.apiKey) == "your-api-key") {
    error = "Configure TTS_API_KEY in include/secret.h";
    return false;
  }
  I2sAudio::Playback playback(speaker_);
  if (!playback.acquired()) { error = "Audio playback is unavailable"; return false; }
  SpiJsonDocument payload;
  payload["model"] = config_.model;
  payload["voice"] = config_.voice;
  payload["input"] = text;
  payload["response_format"] = "pcm";
  String body;
  serializeJson(payload, body);

  // The local audio adapter returns JSON/base64. This endpoint returns binary
  // PCM, streamed here without allocating a full audio response in RAM.
  SpeechClient client;
  client.setInsecure();  // Same TLS policy as the existing AiProviderKit transport.
  HTTPClient http;
  http.setConnectTimeout(10000);
  http.setTimeout(config::requestTimeoutMs);
  http.setReuse(false);
  if (!http.begin(client, String(config_.baseUrl) + "/audio/speech")) {
    error = "TTS connection setup failed";
    return false;
  }
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + config_.apiKey);
  const char* headers[] = {"Content-Type"};
  http.collectHeaders(headers, 1);
  const int code = http.POST(body);
  if (code != 200) {
    error = "TTS HTTP " + std::to_string(code) + ": ";
    if (code < 0) {
      error += HTTPClient::errorToString(code).c_str();
      char tlsError[160] = {};
      if (client.lastError(tlsError, sizeof(tlsError)) != 0 && tlsError[0]) {
        error += "; TLS: ";
        error += tlsError;
      }
    } else {
      error += http.getString().c_str();
    }
    http.end();
    return false;
  }
  const String contentType = http.header("Content-Type");
  if (!contentType.startsWith("audio/pcm") && !contentType.startsWith("application/octet-stream")) {
    error = "TTS expected PCM audio, received: " + std::string(contentType.c_str());
    http.end();
    return false;
  }
  SpeakerStream stream(speaker_, status_);
  const int written = http.writeToStream(&stream);  // Decodes HTTP chunk framing.
  const bool complete = written > 0 && stream.complete();
  http.end();
  std::string finishError;
  const bool drained = speaker_.finish(finishError);
  if (client.timedOut() || !complete) {
    error = client.timedOut() ? "TTS transaction timed out" :
        (stream.error().empty() ? "TTS audio empty, incomplete, or interrupted" : stream.error());
    return false;
  }
  if (!drained) {
    error = finishError;
    return false;
  }
  return true;
}

}  // namespace partner
