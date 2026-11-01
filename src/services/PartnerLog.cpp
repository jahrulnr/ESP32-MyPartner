#include "PartnerLog.h"
#include <Arduino.h>
#include <esp_log.h>
#include <esp_heap_caps.h>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace partner {
PartnerLog log;
PartnerLog* PartnerLog::instance_ = nullptr;
void PartnerLog::begin() {
  void* storage = heap_caps_malloc(sizeof(CircularLog<8192>), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!storage) storage = std::malloc(sizeof(CircularLog<8192>));
  if (storage) buffer_ = new (storage) CircularLog<8192>();
  instance_ = this;
  previous_ = esp_log_set_vprintf(espLog);
}
void PartnerLog::appendFormat(const char* format, va_list args) {
  if (!buffer_) return;
  va_list countArgs; va_copy(countArgs, args);
  const int count = std::vsnprintf(nullptr, 0, format, countArgs);
  va_end(countArgs);
  if (count <= 0) return;
  char* text = static_cast<char*>(std::malloc(static_cast<size_t>(count) + 1));
  if (!text) return;
  va_list writeArgs; va_copy(writeArgs, args);
  std::vsnprintf(text, static_cast<size_t>(count) + 1, format, writeArgs);
  va_end(writeArgs);
  portENTER_CRITICAL(&lock_);
  buffer_->append(text, static_cast<size_t>(count));
  portEXIT_CRITICAL(&lock_);
  std::free(text);
}
void PartnerLog::printf(const char* format, ...) {
  va_list args; va_start(args, format);
  va_list serialArgs; va_copy(serialArgs, args); Serial.vprintf(format, serialArgs); va_end(serialArgs);
  appendFormat(format, args); va_end(args);
}
void PartnerLog::println(const char* message) { printf("%s\r\n", message); }
int PartnerLog::espLog(const char* format, va_list args) {
  PartnerLog* self = instance_;
  if (!self) return 0;
  va_list serialArgs; va_copy(serialArgs, args);
  const int result = self->previous_ ? self->previous_(format, serialArgs) : 0;
  va_end(serialArgs);
  self->appendFormat(format, args);
  return result;
}
std::string PartnerLog::snapshot() {
  if (!buffer_) return {};
  std::string text; text.resize(8192);
  portENTER_CRITICAL(&lock_);
  const size_t count = buffer_->copy(&text[0], text.size());
  portEXIT_CRITICAL(&lock_);
  text.resize(count);
  return text;
}
}  // namespace partner
