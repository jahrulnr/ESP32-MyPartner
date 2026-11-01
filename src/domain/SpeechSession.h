#pragma once

namespace partner {

enum class SpeechFrame { Observe, Recognize, Suppressed };

class SpeechSession {
 public:
  SpeechFrame next(bool speech, bool playbackSuppressed) {
    if (playbackSuppressed) {
      active_ = false;
      return SpeechFrame::Suppressed;
    }
    active_ = active_ || speech;
    return active_ ? SpeechFrame::Recognize : SpeechFrame::Observe;
  }
  void finish() { active_ = false; }

 private:
  bool active_ = false;
};

}  // namespace partner
