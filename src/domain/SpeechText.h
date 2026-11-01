#pragma once
#include <cstddef>
#include <cstdint>

namespace partner {
constexpr size_t kMaxSpeechCharacters = 255;
constexpr size_t kMaxSpeechBytes = kMaxSpeechCharacters * 4;

// JSON strings use UTF-8. Count Unicode code points, not their encoded bytes.
inline bool validSpeechText(const char* text) {
  if (!text || !*text) return false;
  size_t characters = 0, bytes = 0;
  auto* p = reinterpret_cast<const unsigned char*>(text);
  while (*p) {
    const unsigned char first = *p++;
    uint32_t codepoint;
    size_t trailing;
    if (first <= 0x7f) { codepoint = first; trailing = 0; }
    else if (first >= 0xc2 && first <= 0xdf) { codepoint = first & 0x1f; trailing = 1; }
    else if (first >= 0xe0 && first <= 0xef) { codepoint = first & 0x0f; trailing = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { codepoint = first & 0x07; trailing = 3; }
    else return false;
    bytes += trailing + 1;
    if (bytes > kMaxSpeechBytes || ++characters > kMaxSpeechCharacters) return false;
    for (size_t i = 0; i < trailing; ++i) {
      if (!*p || (*p & 0xc0) != 0x80) return false;
      codepoint = (codepoint << 6) | (*p++ & 0x3f);
    }
    if ((trailing == 2 && codepoint < 0x800) ||
        (trailing == 3 && codepoint < 0x10000) ||
        (codepoint >= 0xd800 && codepoint <= 0xdfff) || codepoint > 0x10ffff) return false;
  }
  return true;
}
}  // namespace partner
