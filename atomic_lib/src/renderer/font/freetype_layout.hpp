#pragma once
#include "math/vec.hpp"
#include "renderer/font/interface.hpp"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

namespace ui::font {
class TextLayoutEngine {
public:
  static std::vector<TextRun> parseRichText(const std::string &rawText,
                                            uint32_t defaultSize,
                                            math::vec4<float> defaultColor) {
    std::vector<TextRun> runs;
    runs.push_back({rawText, defaultSize,
                    static_cast<uint8_t>(TextStyleBit::Regular), defaultColor});
    return runs;
  }

  static std::vector<PositionedGlyph>
  calcLayout(const std::vector<ui::font::TextRun> textRuns, Font *fontBackend,
             float maxWidth, float tracking, float lineSpacingScale = 1.0f) {
    std::vector<PositionedGlyph> layoutGlyphs;
    float cursorX = 0.0f;
    float cursorY = 0.0f;
    size_t lastWordBreakIndex = 0;
    float lastWordBreakX = 0.0f;

    for (const auto &run : textRuns) {

      float weightOffset =
          (run.styleFlags & static_cast<uint8_t>(TextStyleBit::Bold)) ? 0.15f
                                                                      : 0.0f;

      for (size_t i = 0; i < run.text.size(); ++i) {
        char c = run.text[i];

        if (c == '\n') {
          cursorX = 0.0f;
          cursorY += fontBackend->getLineHeight() * lineSpacingScale;
          continue;
        }

        if (c == ' ' || c == '\t') {
          lastWordBreakIndex = layoutGlyphs.size();
          lastWordBreakX = cursorX;
        }

        GlyphInfo glyphinfo =
            fontBackend->getGlyphVariant(c, run.fontSize, run.styleFlags);

        float xPos = cursorX + glyphinfo.bearing.x;
        float yPos = cursorY - glyphinfo.bearing.y;
        float width = glyphinfo.size.x;
        float height = glyphinfo.size.y;

        if (maxWidth > 0.0f && (cursorX + glyphinfo.advanceX) > maxWidth) {
          if (lastWordBreakX > 0.0f) {
            float shiftx = lastWordBreakX;
            float shifty = fontBackend->getLineHeight() * lineSpacingScale;

            for (size_t j = lastWordBreakIndex; j < layoutGlyphs.size(); ++j) {
              layoutGlyphs[j].rect.x -= shiftx;
              layoutGlyphs[j].rect.y += shifty;
            }

            cursorX -= shiftx;
            cursorY += shifty;

            xPos = cursorX + glyphinfo.bearing.x;
            yPos = cursorY - glyphinfo.bearing.y;
          } else {
            cursorX = 0.0f;
            cursorY += fontBackend->getLineHeight() * lineSpacingScale;
            xPos = cursorX + glyphinfo.bearing.x;
            yPos = cursorY - glyphinfo.bearing.y;
          }
        }

        PositionedGlyph pg;
        pg.rect = {xPos, yPos, width, height};
        pg.uv = {glyphinfo.uvMin.x, glyphinfo.uvMin.y, glyphinfo.uvMax.x,
                 glyphinfo.uvMax.y};
        pg.color = run.color;
        pg.isText = 1.0f;
        pg.fontWeightOffset = weightOffset;
        layoutGlyphs.push_back(pg);

        cursorX += glyphinfo.advanceX + tracking;
      }
    }

    return layoutGlyphs;
  }
};

} // namespace ui::font
