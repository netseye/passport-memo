#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string>
#include <vector>
#include <math.h>

constexpr size_t NOTE_MAX_CHARS = 240;
constexpr size_t NOTE_MAX_BYTES = NOTE_MAX_CHARS * 4;

// Strict UTF-8 decoding. Never split a code point or accept overlong sequences.
inline bool nextCodepoint(const std::string& text, size_t& pos, uint32_t& cp) {
  if (pos >= text.size()) return false;
  uint8_t c = uint8_t(text[pos++]);
  if (c < 0x80) { cp = c; return true; }
  unsigned more; uint32_t minimum;
  if (c >= 0xC2 && c <= 0xDF) { more = 1; cp = c & 0x1F; minimum = 0x80; }
  else if (c >= 0xE0 && c <= 0xEF) { more = 2; cp = c & 0x0F; minimum = 0x800; }
  else if (c >= 0xF0 && c <= 0xF4) { more = 3; cp = c & 7; minimum = 0x10000; }
  else return false;
  if (pos + more > text.size()) return false;
  while (more--) {
    c = uint8_t(text[pos++]);
    if ((c & 0xC0) != 0x80) return false;
    cp = (cp << 6) | (c & 0x3F);
  }
  return cp >= minimum && cp <= 0x10FFFF && !(cp >= 0xD800 && cp <= 0xDFFF);
}

inline const char* normalizeNote(const std::string& input, std::string& output) {
  output.clear();
  if (input.size() > NOTE_MAX_BYTES + NOTE_MAX_CHARS) return "便签最多240个字符";
  size_t pos = 0, count = 0;
  while (pos < input.size()) {
    size_t start = pos; uint32_t cp;
    if (!nextCodepoint(input, pos, cp)) return "文本不是有效的UTF-8";
    if (cp == '\r') {
      if (pos < input.size() && input[pos] == '\n') ++pos;
      output += '\n';
    } else if (cp == '\t') output += ' ';
    else if ((cp < 32 && cp != '\n') || (cp >= 0x7F && cp <= 0x9F)) return "文本含不支持的控制字符";
    else output.append(input, start, pos - start);
    if (++count > NOTE_MAX_CHARS) return "便签最多240个字符";
  }
  return nullptr;
}

// Width callback measures one already-sanitized UTF-8 glyph in actual pixels.
template <class Measure>
std::vector<std::string> wrapNote(const std::string& text, int maxWidth, Measure measure) {
  std::vector<std::string> lines;
  std::string line; int width = 0;
  for (size_t pos = 0; pos < text.size();) {
    size_t start = pos; uint32_t cp;
    if (!nextCodepoint(text, pos, cp)) break;
    if (cp == '\n') { lines.push_back(line); line.clear(); width = 0; continue; }
    std::string glyph = text.substr(start, pos - start);
    int w = measure(glyph);
    if (!line.empty() && width + w > maxWidth) { lines.push_back(line); line.clear(); width = 0; }
    line += glyph; width += w;
  }
  lines.push_back(line);
  return lines;
}

inline bool validWeatherValues(float temperature, int humidity, float wind, int code,
                               int64_t observed, int64_t received) {
  return isfinite(temperature) && temperature >= -100 && temperature <= 70 &&
         humidity >= 0 && humidity <= 100 && isfinite(wind) && wind >= 0 && wind <= 450 &&
         code >= 0 && code <= 99 && observed >= 1704067200 && received >= 1704067200 &&
         observed <= received + 7200;
}

inline bool weatherIsStale(bool hasData, bool online, bool clockReady, bool restored,
                           bool hasError, int64_t now, int64_t received) {
  return !hasData || !online || !clockReady || restored || hasError ||
         now < received || now - received > 1800;
}
