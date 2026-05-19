#pragma once
#include <cstdint>
#include <functional>
#include <math/vec.hpp>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui::font {
enum class TextStyleBit : uint8_t {
  Regular = 0,
  Bold = 1 << 0,
  Italic = 1 << 1,
};

struct TextRun {
  std::string text;
  uint32_t fontSize;
  uint8_t styleFlags;
  math::vec4<float> color;
};

struct PositionedGlyph {
  math::vec4<float> rect;
  math::vec4<float> uv;
  math::vec4<float> color;
  float isText;
  float fontWeightOffset;
};

struct GlyphKey {
  char32_t codepoint;
  uint32_t fontSize;
  uint32_t fontWeight;

  bool operator==(const GlyphKey &o) const {
    return codepoint == o.codepoint && fontSize == o.fontSize &&
           fontWeight == o.fontWeight;
  }
};

struct GlyphInfo {};

class Font {
public:
  virtual ~Font() = default;
  virtual bool load(const std::string &path, uint32_t size) = 0;
  virtual GlyphInfo getGlyphVariant(char32_t codepoint, uint32_t size,
                                    uint8_t styleFlags) = 0;

  virtual float getLineHeight() const = 0;
  virtual float getAscender() const = 0;

  virtual void *getTextureHandle() = 0;
};
} // namespace ui::font

namespace std {
template <> struct hash<ui::font::GlyphKey> {
  size_t operator()(const ui::font::GlyphKey &k) const {
    return ((hash<uint32_t>()(k.codepoint) ^
             (hash<uint32_t>()(k.fontSize) << 1)) >>
            1) ^
           (hash<uint32_t>()(k.fontWeight) << 1);
  }
};
} // namespace std
