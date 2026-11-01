#include "domain/Pcm24To16.h"
#include <cassert>
#include <cstring>
#include <iostream>
#include <vector>

using namespace partner;
struct Sink : IPcmSink {
  std::vector<int16_t> values;
  bool writePcm(const uint8_t* bytes, size_t size, std::string&) override {
    for (size_t i = 0; i < size; i += 2) {
      int16_t sample;
      std::memcpy(&sample, bytes + i, 2);
      values.push_back(sample);
    }
    return true;
  }
};
int main() {
  const std::vector<int16_t> input = {0, 100, 200, 300, 400, 500, -32768, -32768, -32768};
  const std::vector<int16_t> expected = {0, 150, 300, 450, -32768, -32768};
  const auto* bytes = reinterpret_cast<const uint8_t*>(input.data());
  for (size_t split = 0; split <= input.size() * 2; ++split) {
    Sink sink;
    Pcm24To16 converter(sink);
    Pcm16Assembler assembler(converter);
    std::string error;
    assert(assembler.write(bytes, split, error));
    assert(assembler.write(bytes + split, input.size() * 2 - split, error));
    assert(assembler.complete() && converter.finish(error));
    assert(sink.values == expected);
  }
  Sink sink;
  Pcm24To16 converter(sink);
  std::string error;
  std::vector<int16_t> oneSecond(24000, 1000);
  assert(converter.writePcm(reinterpret_cast<uint8_t*>(oneSecond.data()), 48000, error));
  assert(converter.finish(error));
  assert(sink.values.size() == 16000);
  for (const auto sample : sink.values) assert(sample == 1000);
  std::cout << "resample: 24/16 kHz duration, interpolation, extrema and split samples passed\n";
}
