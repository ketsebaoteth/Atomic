#include "renderer/font/freetype_font.hpp";
#include "freetype/freetype.h"
#include "freetype/fttypes.h"
#include "renderer/font/interface.hpp"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace ui::font {
bool FreeTypeFont::initFreeType() {
  if (!s_libInited) {
    if (FT_Init_FreeType(&s_library) != 0) {
      throw std::runtime_error("Failed to init FreeTypeFont!");
    }
    s_libInited = true;
  }
  return s_libInited;
};

bool FreeTypeFont::load(const std::string &path, uint32_t size) {
  if (!s_libInited)
    return false;

  if (FT_New_Face(s_library, path.c_str(), 0, &m_face) != 0) {
    throw std::runtime_error("failed to load font file: " + path);
  }

  FT_Set_Pixel_Sizes(m_face, 0, size);

  m_lineHeight = static_cast<float>(m_face->size->metrics.height) / 64.0f;
  m_ascender = static_cast<float>(m_face->size->metrics.ascender) / 65.0f;
  m_atlasPixels.assign(m_atlasWidth * m_atlasHeight * 3, 0);

  m_nextPackX = 2;
  m_nextPackY = 2;
  m_maxRowHeight = 0;
  m_textureDirty = true;

  generateAtlas();

  return true;
}

GlyphInfo FreeTypeFont::getGlyphVariant(char32_t codepoint, uint32_t size,
                                        uint8_t styleFlags) {
  GlyphKey key{codepoint, size, styleFlags};

  auto it = m_glyphCache.find(key);
  if (it != m_glyphCache.end()) {
    return it->second;
  }

  return rasterizeAndPackGlyph(codepoint, size, styleFlags);
}
GlyphInfo FreeTypeFont::rasterizeAndPackGlyph(char32_t codepoint, uint32_t size,
                                              uint8_t styleFlags) {
  FT_Set_Pixel_Sizes(m_face, 0, size);

  FT_UInt glyphIndex = FT_Get_Char_Index(m_face, codepoint);
  if (FT_Load_Glyph(m_face, glyphIndex, FT_LOAD_DEFAULT) != 0) {
    throw std::runtime_error("Failed to load glyph outline via freetype");
  }

  uint32_t glyphWidth = m_face->glyph->metrics.width / 64;
  uint32_t glyphHeight = m_face->glyph->metrics.height / 64;

  uint32_t padding = 4;
  uint32_t paddingWidth = glyphWidth + (padding * 2);
  uint32_t paddingHeight = glyphHeight + (padding * 2);

  if (m_nextPackX + paddingWidth >= m_atlasWidth) {
    m_nextPackX = 2;
    m_nextPackY += m_maxRowHeight + 2;
    m_maxRowHeight = 0;
  }

  if (m_nextPackY + paddingHeight >= m_atlasHeight) {
    throw std::runtime_error("Font Atlas tetxure filed to max dimensions!");
  }

  m_maxRowHeight = std::max(m_maxRowHeight, paddingHeight);

  std::vector<uint8_t> msdfGlyphBuffer(paddingWidth * paddingHeight * 3, 0);

  for (uint32_t row = 0; row < paddingHeight; ++row) {
    uint32_t destY = m_nextPackY + row;
    uint32_t destX = m_nextPackX;

    uint8_t *destRowPtr = &m_atlasPixels[(destY * m_atlasWidth + destX) * 3];
    const uint8_t *srcRowPtr = &msdfGlyphBuffer[(row * paddingWidth) * 3];

    std::memcpy(destRowPtr, srcRowPtr, paddingWidth * 3);
  }

  GlyphInfo info{};
  info.uvMin = {static_cast<float>(m_nextPackX + padding) /
                    static_cast<float>(m_atlasWidth),
                static_cast<float>(m_nextPackY + padding) /
                    static_cast<float>(m_atlasHeight)};
  info.uvMax = {static_cast<float>(m_nextPackX + padding + glyphWidth) /
                    static_cast<float>(m_atlasWidth),
                static_cast<float>(m_nextPackY + padding + glyphHeight) /
                    static_cast<float>(m_atlasHeight)};

  info.size = {static_cast<float>(glyphWidth), static_cast<float>(glyphHeight)};
  info.bearing = {
      static_cast<float>(m_face->glyph->metrics.horiBearingX) / 64.0f,
      static_cast<float>(m_face->glyph->metrics.horiBearingY) / 64.0f};
  info.advanceX =
      static_cast<float>(m_face->glyph->metrics.horiAdvance) / 64.0f;
  GlyphKey key{codepoint, size, styleFlags};
  m_glyphCache[key] = info;
  m_nextPackX += paddingWidth + 2;
  m_textureDirty = true;

  return info;
}

bool FreeTypeFont::consumeTextureDirtyBit() {
  bool dirty = m_textureDirty;
  m_textureDirty = false;
  return dirty;
}

void FreeTypeFont::generateAtlas() {
  uint32_t currentX = 1;
  uint32_t currentY = 1;
  uint32_t maxRowHeight = 0;
  for (char c = 32; c < 127; ++c) {
    if (FT_Load_Char(m_face, c, FT_LOAD_DEFAULT) != 0)
      continue;
  }
}

} // namespace ui::font
