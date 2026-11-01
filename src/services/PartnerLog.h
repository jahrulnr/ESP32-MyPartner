#pragma once
#include <cstdarg>
#include <string>
#include <freertos/FreeRTOS.h>
#include "domain/CircularLog.h"

namespace partner {
class PartnerLog {
 public:
  void begin();
  void printf(const char* format, ...) __attribute__((format(printf, 2, 3)));
  void println(const char* message);
  std::string snapshot();
 private:
  void appendFormat(const char* format, va_list args);
  static int espLog(const char* format, va_list args);
  static PartnerLog* instance_;
  using LogOutput = int (*)(const char*, va_list);
  LogOutput previous_ = nullptr;
  portMUX_TYPE lock_ = portMUX_INITIALIZER_UNLOCKED;
  CircularLog<8192>* buffer_ = nullptr;
};
extern PartnerLog log;
}  // namespace partner
