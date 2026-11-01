#pragma once
#include <cstdint>
#include <cstring>

namespace partner {
enum class Screen : uint8_t { Home, Clock, Sensors };
enum class Expression : uint8_t { Auto, Normal, Happy, Sad, Worried, Focused, Surprised, Angry, Sleepy };

inline const char* screenName(Screen value) {
  switch (value) { case Screen::Home: return "home"; case Screen::Clock: return "clock"; case Screen::Sensors: return "sensors"; }
  return "home";
}
inline bool parseScreen(const char* text, Screen& out) {
  if (!text) return false;
  for (unsigned i = 0; i < 3; ++i) {
    const auto value = static_cast<Screen>(i);
    if (std::strcmp(text, screenName(value)) == 0) { out = value; return true; }
  }
  return false;
}
inline const char* expressionName(Expression value) {
  switch (value) {
    case Expression::Auto: return "auto"; case Expression::Normal: return "normal";
    case Expression::Happy: return "happy"; case Expression::Sad: return "sad";
    case Expression::Worried: return "worried"; case Expression::Focused: return "focused";
    case Expression::Surprised: return "surprised"; case Expression::Angry: return "angry";
    case Expression::Sleepy: return "sleepy";
  }
  return "auto";
}
inline bool parseExpression(const char* text, Expression& out) {
  if (!text) return false;
  for (unsigned i = 0; i < 9; ++i) {
    const auto value = static_cast<Expression>(i);
    if (std::strcmp(text, expressionName(value)) == 0) { out = value; return true; }
  }
  return false;
}
}  // namespace partner
