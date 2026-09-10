// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
namespace tagtinker {
// Commands are flat objects. Reject nesting before the recursive JSON parser runs
// on the small ESP32 loop stack; quoted braces remain valid text.
inline bool flatJsonEnvelope(const std::string& line) {
  if (line.empty() || line.size() > 1024) return false;
  unsigned objects = 0;
  bool quoted = false, escaped = false;
  for (char c : line) {
    if (escaped) { escaped = false; continue; }
    if (quoted && c == '\\') { escaped = true; continue; }
    if (c == '"') { quoted = !quoted; continue; }
    if (quoted) continue;
    if (c == '[' || c == ']') return false;
    if (c == '{' && ++objects != 1) return false;
    if (c == '}' && objects-- != 1) return false;
  }
  return !quoted && !escaped && objects == 0;
}
}
