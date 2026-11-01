#include "domain/GreetingGate.h"
#include "domain/SpeechSession.h"
#include <cassert>
#include <iostream>
#include <limits>
using namespace partner;
int main() {
  SpeechSession context;
  // Quiet frames still supply acoustic context before VAD confirms the initial H.
  assert(context.next(false, false) == SpeechFrame::Observe);
  SpeechSession session;
  assert(session.next(false, false) == SpeechFrame::Observe);
  assert(session.next(true, false) == SpeechFrame::Recognize);
  // Keep the pause between HI and PARTNER, plus trailing silence, in one stream.
  assert(session.next(false, false) == SpeechFrame::Recognize);
  assert(session.next(true, false) == SpeechFrame::Recognize);
  assert(session.next(false, false) == SpeechFrame::Recognize);
  session.finish();  // MultiNet detection or timeout closes the window.
  assert(session.next(false, false) == SpeechFrame::Observe);
  assert(session.next(true, false) == SpeechFrame::Recognize);
  assert(session.next(true, true) == SpeechFrame::Suppressed);
  assert(session.next(false, false) == SpeechFrame::Observe);
  assert(session.next(true, true) == SpeechFrame::Suppressed);
  assert(session.next(true, false) == SpeechFrame::Recognize);
  GreetingGate acoustic(90);
  assert(!acoustic.accept(0, false, 0.522f, 75));  // Observed drama false positive.
  assert(!acoustic.accept(1, false, 0.99f, 89));
  assert(acoustic.accept(2, false, 0.386f, 90));  // Nearby voice at the boundary.
  GreetingGate recordedSpeech(90);
  assert(!recordedSpeech.accept(0, false, 0.349f, 110));
  assert(recordedSpeech.accept(1, false, 0.391f, 110));  // Real replay score below the old 0.4 threshold.
  GreetingGate gate(90);
  assert(!gate.accept(10, false, 0.269f, 110));  // Observed score for the silent-input false positive.
  assert(!gate.accept(10, false, std::numeric_limits<float>::quiet_NaN(), 110));
  assert(!gate.accept(10, true, 0.99f, 110));
  assert(gate.accept(20, false, 0.99f, 110));
  assert(!gate.accept(30, false, 0.99f, 110));
  assert(!gate.accept(1220, true, 0.99f, 110));
  assert(gate.accept(1220, false, 0.99f, 110));
  GreetingGate rollover(90);
  assert(rollover.accept(UINT32_MAX - 500, false, 0.99f, 110));
  assert(!rollover.accept(100, false, 0.99f, 110));
  assert(rollover.accept(1000, false, 0.99f, 110));
  std::cout << "greeting: utterance session, suppression, duplicate detection, rearm and clock rollover passed\n";
}
