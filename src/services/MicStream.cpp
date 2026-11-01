#include "MicStream.h"
#include "services/PartnerLog.h"
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <algorithm>
#include <cstring>

namespace partner {

bool MicStream::begin() {
  frameStorage_ = static_cast<uint8_t*>(heap_caps_malloc(8 * sizeof(Frame), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  frames_ = frameStorage_ ? xQueueCreateStatic(8, sizeof(Frame), frameStorage_, &frameControl_) : xQueueCreate(8, sizeof(Frame));
  if (!frames_) return false;
  server_.onEvent([this](uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
    event(client, type, payload, length);
  });
  return true;
}

void MicStream::publish(const int16_t* samples, size_t count) {
  if (!frames_ || !active_.load()) {
    producer_.count = 0;
    return;
  }
  while (count && active_.load()) {
    const uint32_t generation = generation_.load();
    if (producer_.generation != generation) producer_.count = 0;
    if (!producer_.count) {
      producer_.generation = generation;
      producer_.sampleIndex = sampleIndex_.load();
    }
    const size_t copied = std::min(count, maxSamples - producer_.count);
    std::memcpy(producer_.pcm + producer_.count, samples, copied * sizeof(int16_t));
    producer_.count += copied;
    sampleIndex_.fetch_add(copied);
    samples += copied;
    count -= copied;
    if (producer_.count == maxSamples) {
      if (xQueueSend(frames_, &producer_, 0) != pdTRUE) dropped_.fetch_add(1);
      producer_.count = 0;
    }
  }
}

void MicStream::stop(int requester) {
  const int previousOwner = owner_;
  active_.store(false);
  generation_.fetch_add(1);
  xQueueReset(frames_);
  owner_ = -1;
  char status[96];
  snprintf(status, sizeof(status), "{\"type\":\"stopped\",\"dropped_frames\":%u}",
           static_cast<unsigned>(dropped_.load()));
  if (previousOwner >= 0 && server_.clientIsConnected(previousOwner)) server_.sendTXT(previousOwner, status);
  if (requester >= 0 && requester != previousOwner) server_.sendTXT(requester, status);
  partner::log.printf("[mic-ws] stopped, dropped_frames=%u\n", static_cast<unsigned>(dropped_.load()));
}

void MicStream::event(uint8_t client, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_DISCONNECTED) {
    if (owner_ == client) stop(-1);
    return;
  }
  if (type == WStype_CONNECTED) {
    server_.sendTXT(client, "{\"type\":\"ready\",\"format\":\"pcm_s16le\",\"rate\":16000,\"channels\":1,\"header\":\"sample_index_u32le\"}");
    return;
  }
  if (type != WStype_TEXT) return;
  if (length == 5 && std::memcmp(payload, "START", 5) == 0) {
    if (active_.load() && owner_ != client) {
      server_.sendTXT(client, "{\"type\":\"error\",\"message\":\"stream busy\"}");
      return;
    }
    if (!active_.load()) {
      xQueueReset(frames_);
      generation_.fetch_add(1);
      dropped_.store(0);
      owner_ = client;
      active_.store(true);
      partner::log.printf("[mic-ws] started client=%u\n", client);
    }
    server_.sendTXT(client, "{\"type\":\"started\",\"format\":\"pcm_s16le\",\"rate\":16000,\"channels\":1,\"header\":\"sample_index_u32le\"}");
  } else if (length == 4 && std::memcmp(payload, "STOP", 4) == 0) {
    stop(client);
  } else if (length == 6 && std::memcmp(payload, "STATUS", 6) == 0) {
    server_.sendTXT(client, active_.load() ? "{\"type\":\"status\",\"active\":true}" : "{\"type\":\"status\",\"active\":false}");
  } else {
    server_.sendTXT(client, "{\"type\":\"error\",\"message\":\"use START, STOP or STATUS\"}");
  }
}

void MicStream::tick() {
  if (!frames_) return;
  if (WiFi.status() != WL_CONNECTED) {
    if (active_.load()) stop(-1);
    return;
  }
  if (!listening_) {
    server_.begin();
    listening_ = true;
  }
  if (WiFi.localIP() != announcedIp_) {
    announcedIp_ = WiFi.localIP();
    partner::log.printf("[mic-ws] ready: ws://%s:81/ (stream off)\n", announcedIp_.toString().c_str());
  }
  server_.loop();
  for (int i = 0; i < 4 && xQueueReceive(frames_, &outgoing_, 0) == pdTRUE; ++i) {
    if (!active_.load() || outgoing_.generation != generation_.load() || owner_ < 0) continue;
    for (unsigned b = 0; b < 4; ++b) packet_[b] = outgoing_.sampleIndex >> (8 * b);
    std::memcpy(packet_ + 4, outgoing_.pcm, outgoing_.count * sizeof(int16_t));
    if (!server_.sendBIN(owner_, packet_, 4 + outgoing_.count * sizeof(int16_t))) {
      stop(-1);
      break;
    }
  }
}

}  // namespace partner
