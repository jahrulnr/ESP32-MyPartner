#pragma once

#include <cstdint>
#include <string>

namespace partner {

struct LocalTime {
  int64_t epoch = 0;
  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;
  bool valid = false;

  int64_t hourId() const { return epoch / 3600; }
};

enum class ReminderPhase : uint8_t { Idle, Generating, Synthesizing, Speaking, Error };

// A fresh clock establishes a baseline. Forward jumps announce only the newest
// hour; backward corrections cannot announce an already observed hour twice.
class HourlySchedule {
 public:
  bool update(const LocalTime& now) {
    if (!now.valid) return false;
    if (!initialized_) {
      initialized_ = true;
      latestHour_ = now.hourId();
      return false;
    }
    if (now.hourId() <= latestHour_) return false;
    latestHour_ = now.hourId();
    return true;
  }

 private:
  bool initialized_ = false;
  int64_t latestHour_ = 0;
};

class IReminderText {
 public:
  virtual ~IReminderText() = default;
  virtual bool generate(const LocalTime& now, std::string& text, std::string& error) = 0;
};

class IReminderStatus {
 public:
  virtual ~IReminderStatus() = default;
  virtual void setPhase(ReminderPhase phase, const std::string& error = {}) = 0;
};

class ISpeechOutput {
 public:
  virtual ~ISpeechOutput() = default;
  virtual bool speak(const std::string& text, std::string& error) = 0;
};

class ReminderWorkflow {
 public:
  ReminderWorkflow(IReminderText& generator, ISpeechOutput& speech, IReminderStatus& status)
      : generator_(generator), speech_(speech), status_(status) {}

  bool run(const LocalTime& now) {
    if (!now.valid) return false;
    std::string text;
    std::string error;
    status_.setPhase(ReminderPhase::Generating);
    if (!generator_.generate(now, text, error) || text.empty()) {
      status_.setPhase(ReminderPhase::Error, error.empty() ? "LLM returned no text" : error);
      return false;
    }
    status_.setPhase(ReminderPhase::Synthesizing);
    if (!speech_.speak(text, error)) {
      status_.setPhase(ReminderPhase::Error, error);
      return false;
    }
    status_.setPhase(ReminderPhase::Idle);
    return true;
  }

 private:
  IReminderText& generator_;
  ISpeechOutput& speech_;
  IReminderStatus& status_;
};

}  // namespace partner
