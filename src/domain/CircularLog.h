#pragma once
#include <cstddef>
#include <cstring>

namespace partner {
template <size_t Capacity> class CircularLog {
 public:
  void append(const char* text, size_t count) {
    for (size_t i = 0; i < count; ++i) {
      buffer_[next_] = text[i]; next_ = (next_ + 1) % Capacity;
      if (size_ < Capacity) ++size_;
    }
  }
  size_t copy(char* out, size_t capacity) const {
    const size_t count = size_ < capacity ? size_ : capacity;
    const size_t start = (next_ + Capacity - count) % Capacity;
    for (size_t i = 0; i < count; ++i) out[i] = buffer_[(start + i) % Capacity];
    return count;
  }
  size_t size() const { return size_; }
 private:
  char buffer_[Capacity] {};
  size_t next_ = 0, size_ = 0;
};
}  // namespace partner
