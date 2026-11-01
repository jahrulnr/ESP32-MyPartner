#include "domain/PersonalReminders.h"
#include <cassert>
#include <cstring>
#include <iostream>
using namespace partner;
int main() {
  PresenceTransitions presence;
  assert(!presence.update(false, false));
  assert(!presence.update(true, true));
  assert(!presence.update(true, false));
  assert(presence.update(true, true));
  assert(!presence.update(false, false));
  assert(!presence.update(true, true));
  assert(!presence.update(true, false));
  assert(!presence.update(false, false));
  assert(!presence.update(true, true));
  PersonalReminders reminders;
  uint32_t a, b;
  assert(reminders.add(200, "Istirahat", a));
  assert(reminders.add(100, "Minum air", b));
  PersonalReminder job;
  assert(!reminders.takeDue(150, false, job));
  assert(reminders.takeDue(150, true, job));
  assert(job.id == b && std::strcmp(job.text, "Minum air") == 0);
  assert(!reminders.takeDue(250, true, job));
  assert(!reminders.remove(b));
  assert(reminders.finish(b, false));
  assert(reminders.takeDue(250, true, job) && job.id == b);
  assert(reminders.finish(b, true));
  assert(reminders.takeDue(250, true, job) && job.id == a);
  assert(reminders.finish(a, true));
  assert(!reminders.takeDue(250, true, job));
  PersonalReminder saved {44, 900, "Tugas"};
  assert(reminders.restore(saved));
  assert(!reminders.restore(saved));
  assert(reminders.add(901, "Lainnya", a) && a > 44);
  std::cout << "personal reminders: arrival, due ordering, deferral and delivery passed\n";
}
