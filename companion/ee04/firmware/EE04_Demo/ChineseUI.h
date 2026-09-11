#pragma once
#include <U8g2_for_Adafruit_GFX.h>
#include "ContentLogic.h"

class ChineseUI {
 public:
  U8G2_FOR_ADAFRUIT_GFX font;
  void begin(Adafruit_GFX& display) {
    font.begin(display);
    font.setFontMode(1);
    font.setFontDirection(0);
    font.setForegroundColor(0);
    font.setBackgroundColor(0xFFFF);
    font.setFont(u8g2_font_wqy16_t_gb2312);
  }
  void small(bool yes = false) {
    font.setFont(yes ? u8g2_font_wqy12_t_gb2312 : u8g2_font_wqy16_t_gb2312);
  }
  std::string supported(const std::string& input) {
    std::string out;
    for (size_t p = 0; p < input.size();) {
      size_t start = p; uint32_t cp;
      if (!nextCodepoint(input, p, cp)) break;
      std::string glyph = input.substr(start, p - start);
      if (cp == '\n') out += '\n';
      else if (cp > 0xFFFF || font.getUTF8Width(glyph.c_str()) <= 0) out += '?';
      else out += glyph;
    }
    return out;
  }
  std::string fit(const std::string& input, int pixels) {
    std::string clean = supported(input), out;
    for (size_t p = 0; p < clean.size();) {
      size_t start = p; uint32_t cp;
      if (!nextCodepoint(clean, p, cp)) break;
      if (cp == '\n') break;
      std::string candidate = out + clean.substr(start, p - start);
      if (font.getUTF8Width(candidate.c_str()) > pixels) return out;
      out = candidate;
    }
    return out;
  }
  void text(int x, int baseline, const std::string& value, int maxWidth = 280) {
    font.setCursor(x, baseline);
    font.print(fit(value, maxWidth).c_str());
  }
  std::vector<std::string> wrap(const std::string& value, int width = 276) {
    small();
    return wrapNote(supported(value), width, [this](const std::string& s) {
      return font.getUTF8Width(s.c_str());
    });
  }
};
