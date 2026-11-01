#include "domain/Pcm16.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace partner;

struct Sink : IPcmSink {
  std::vector<uint8_t> output;
  bool ok = true;
  bool writePcm(const uint8_t* bytes, size_t size, std::string& error) override {
    assert(size % 2 == 0);
    if (!ok) { error = "I2S failed"; return false; }
    output.insert(output.end(), bytes, bytes + size);
    return true;
  }
};

int main() {
  const std::vector<uint8_t> audio = {0x00, 0x00, 0xff, 0x7f, 0x00, 0x80, 0x01, 0x00};
  for (size_t split = 0; split <= audio.size(); ++split) {
    Sink sink;
    Pcm16Assembler pcm(sink);
    std::string error;
    assert(pcm.write(audio.data(), split, error));
    assert(pcm.write(audio.data() + split, audio.size() - split, error));
    assert(pcm.complete());
    assert(sink.output == audio);
  }
  Sink sink;
  Pcm16Assembler pcm(sink);
  std::string error;
  for (uint8_t byte : audio) assert(pcm.write(&byte, 1, error));
  assert(sink.output == audio && pcm.complete());
  assert(pcm.write(audio.data(), 1, error));
  assert(!pcm.complete());
  sink.ok = false;
  assert(!pcm.write(audio.data(), 1, error));
  assert(error == "I2S failed");
  std::cout << "pcm16: arbitrary chunk boundaries, zero bytes and write failure passed\n";
}
