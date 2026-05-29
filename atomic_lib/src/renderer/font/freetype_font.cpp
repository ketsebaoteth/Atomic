#include "renderer/font/freetype_font.hpp"
#include <cmath>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <fontconfig/fontconfig.h>
#include <stdexcept>
#include <vector>

namespace ui::font {

bool FreeTypeFont::s_libInited = false;
FT_Library FreeTypeFont::s_library = nullptr;

bool FreeTypeFont::initFreeType() {
  if (!s_libInited) {
    if (FT_Init_FreeType(&s_library) != 0) {
      throw std::runtime_error("Failed to init FreeTypeFont library instance!");
    }
    s_libInited = true;
  }
  return s_libInited;
}

static std::string resolveFont(const std::string &name) {
  FcInit();

  FcPattern *pattern = FcNameParse((const FcChar8 *)name.c_str());

  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match = FcFontMatch(nullptr, pattern, &result);

  FcPatternDestroy(pattern);

  if (!match) {
    return "";
  }

  FcChar8 *file = nullptr;

  if (FcPatternGetString(match, FC_FILE, 0, &file) != FcResultMatch) {
    FcPatternDestroy(match);
    return "";
  }

  std::string path = (char *)file;

  FcPatternDestroy(match);

  return path;
}

bool FreeTypeFont::load(const std::string &path, uint32_t size) {
  if (!s_libInited) {
    initFreeType();
  }

  std::string actualPath = path;

  // Try exact path/file first
  if (FT_New_Face(s_library, actualPath.c_str(), 0, &m_face) != 0) {

    // Try system font lookup
    actualPath = resolveFont(path);

    if (actualPath.empty() ||
        FT_New_Face(s_library, actualPath.c_str(), 0, &m_face) != 0) {

      // Final fallback
      actualPath = resolveFont("inter");

      if (actualPath.empty() ||
          FT_New_Face(s_library, actualPath.c_str(), 0, &m_face) != 0) {

        throw std::runtime_error("Failed to load any usable font");
      }
    }
  }

  m_initialSize = size;

  m_atlasPixels.assign(m_atlasWidth * m_atlasHeight, 0);

  m_nextPackX = 2;
  m_nextPackY = 2;
  m_maxRowHeight = 0;
  m_textureDirty = true;

  generateAtlas();

  return true;
}

// ----------------------------------------------------------------------
// Dynamic Metric Lookups
// ----------------------------------------------------------------------
float FreeTypeFont::getLineHeight(float size) const {
  FT_F26Dot6 fixedSize = static_cast<FT_F26Dot6>(std::round(size * 64.0f));
  FT_Set_Char_Size(const_cast<FT_Face>(m_face), 0, fixedSize, 72, 72);

  return static_cast<float>(m_face->size->metrics.height) / 64.0f;
}

float FreeTypeFont::getAscender(float size) const {
  FT_F26Dot6 fixedSize = static_cast<FT_F26Dot6>(std::round(size * 64.0f));
  FT_Set_Char_Size(const_cast<FT_Face>(m_face), 0, fixedSize, 72, 72);

  return static_cast<float>(m_face->size->metrics.ascender) / 64.0f;
}

GlyphInfo FreeTypeFont::getGlyphVariant(char32_t codepoint, float size,
                                        uint8_t styleFlags) {
  // Quantize incoming subpixel float size into stable 26.6 representation for
  // cache lookup
  int32_t fixedSize = static_cast<int32_t>(std::round(size * 64.0f));
  GlyphKey key{codepoint, fixedSize, styleFlags};

  auto it = m_glyphCache.find(key);
  if (it != m_glyphCache.end()) {
    return it->second;
  }

  return rasterizeAndPackGlyph(codepoint, size, styleFlags);
}

GlyphInfo FreeTypeFont::rasterizeAndPackGlyph(char32_t codepoint, float size,
                                              uint8_t styleFlags) {
  // Set subpixel dimensions natively in FreeType to safely retrieve clean
  // layout metrics
  FT_F26Dot6 fixedSize = static_cast<FT_F26Dot6>(std::round(size * 64.0f));
  FT_Set_Char_Size(m_face, 0, fixedSize, 72, 72);

  if (FT_Load_Char(m_face, codepoint, FT_LOAD_RENDER) != 0) {
    throw std::runtime_error(
        "Failed to render glyph character via FreeType context!");
  }

  FT_Bitmap &bitmap = m_face->glyph->bitmap;
  uint32_t glyphWidth = bitmap.width;
  uint32_t glyphRows = bitmap.rows;

  if (m_nextPackX + glyphWidth + 2 >= m_atlasWidth) {
    m_nextPackX = 2;
    m_nextPackY += m_maxRowHeight + 2;
    m_maxRowHeight = 0;
  }

  if (m_nextPackY + glyphRows + 2 >= m_atlasHeight) {
    throw std::runtime_error(
        "Vulkan UI Error: Font Atlas texture filled to maximum dimensions!");
  }

  m_maxRowHeight = std::max(m_maxRowHeight, glyphRows);

  for (uint32_t row = 0; row < glyphRows; ++row) {
    uint32_t destY = m_nextPackY + row;
    uint32_t destX = m_nextPackX;

    std::memcpy(&m_atlasPixels[destY * m_atlasWidth + destX],
                &bitmap.buffer[row * bitmap.width], glyphWidth);
  }

  GlyphInfo info{};
  info.uvMin = {
      static_cast<float>(m_nextPackX) / static_cast<float>(m_atlasWidth),
      static_cast<float>(m_nextPackY) / static_cast<float>(m_atlasHeight)};
  info.uvMax = {static_cast<float>(m_nextPackX + glyphWidth) /
                    static_cast<float>(m_atlasWidth),
                static_cast<float>(m_nextPackY + glyphRows) /
                    static_cast<float>(m_atlasHeight)};

  info.size = {static_cast<float>(glyphWidth), static_cast<float>(glyphRows)};
  info.bearing = {static_cast<float>(m_face->glyph->bitmap_left),
                  static_cast<float>(m_face->glyph->bitmap_top)};

  // Extract sub-pixel text advance width from the 16.16 fixed-point metrics
  // slot
  info.advanceX = static_cast<float>(m_face->glyph->advance.x) / 64.0f;

  // Key matching the exact structure declaration using fixed size
  GlyphKey key{codepoint, static_cast<int32_t>(fixedSize), styleFlags};
  m_glyphCache[key] = info;

  m_nextPackX += glyphWidth + 2;
  m_textureDirty = true;

  return info;
}

bool FreeTypeFont::consumeTextureDirtyBit() {
  bool dirty = m_textureDirty;
  m_textureDirty = false;
  return dirty;
}

void FreeTypeFont::generateAtlas() {
  // Convert physical configuration baseline size cleanly to launch
  // configuration warmup sequence
  float baseWarmupSize = static_cast<float>(m_initialSize);
  for (char c = 32; c < 127; ++c) {
    rasterizeAndPackGlyph(static_cast<char32_t>(c), baseWarmupSize, 0);
  }
}

} // namespace ui::font
