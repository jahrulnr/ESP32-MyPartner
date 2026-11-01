#include "domain/Reminder.h"

#include <cassert>
#include <iostream>
#include <vector>

using namespace partner;

static LocalTime at(int64_t epoch) {
  LocalTime now;
  now.epoch = epoch;
  now.valid = true;
  return now;
}

struct Text : IReminderText {
  bool ok = true;
  std::string output = "Sekarang pukul sembilan WIB.";
  int calls = 0;
  bool generate(const LocalTime&, std::string& text, std::string& error) override {
    ++calls;
    text = output;
    if (!ok) error = "LLM unavailable";
    return ok;
  }
};

struct Speech : ISpeechOutput {
  bool ok = true;
  int calls = 0;
  std::string received;
  bool speak(const std::string& text, std::string& error) override {
    ++calls;
    received = text;
    if (!ok) error = "Speaker unavailable";
    return ok;
  }
};

struct Status : IReminderStatus {
  std::vector<ReminderPhase> phases;
  std::string error;
  void setPhase(ReminderPhase phase, const std::string& message = {}) override {
    phases.push_back(phase);
    error = message;
  }
};

int main() {
  HourlySchedule schedule;
  assert(!schedule.update({}));
  assert(!schedule.update(at(3599)));  // First NTP sync does not speak mid-hour.
  assert(schedule.update(at(3600)));
  assert(!schedule.update(at(3600)));
  assert(!schedule.update(at(7199)));
  assert(schedule.update(at(7200)));
  assert(!schedule.update(at(3500)));  // NTP moved backwards.
  assert(!schedule.update(at(7200)));
  assert(schedule.update(at(5 * 3600)));  // Skip missed hours without a burst.
  assert(schedule.update(at(24 * 3600)));  // Midnight is a new hour.
  assert(!schedule.update({}));
  assert(!schedule.update(at(24 * 3600 + 1)));

  Text generator;
  Speech speech;
  Status status;
  ReminderWorkflow workflow(generator, speech, status);
  assert(!workflow.run({}));
  assert(generator.calls == 0 && speech.calls == 0);
  assert(workflow.run(at(3600)));
  assert(speech.received == generator.output);
  assert((status.phases == std::vector<ReminderPhase>{ReminderPhase::Generating,
      ReminderPhase::Synthesizing, ReminderPhase::Idle}));

  status.phases.clear();
  generator.ok = false;
  assert(!workflow.run(at(7200)));
  assert(speech.calls == 1);
  assert(status.phases.back() == ReminderPhase::Error);
  assert(status.error == "LLM unavailable");

  generator.ok = true;
  generator.output.clear();
  assert(!workflow.run(at(7200)));
  assert(speech.calls == 1);
  assert(status.error == "LLM returned no text");

  generator.output = "Sekarang tengah malam WIB.";
  speech.ok = false;
  assert(!workflow.run(at(86400)));
  assert(status.error == "Speaker unavailable");
  std::cout << "reminder: schedule, corrections, rollover, pipeline and failures passed\n";
}
