#pragma once
#include <cstdint>
#include <ft2build.h>
#include <renderer/font/interface.hpp>
#include <vector>
#include FT_FREETYPE_H
#include <unordered_map>

namespace ui::font {
class FreeTypeFont : Font {
private:
  FT_Face m_face = nullptr;
  static FT_Library s_library;
  static bool s_libInited;
  std::unordered_map<GlyphKey, GlyphInfo> m_glyphCache;
  void *m_vulkanTextureHandle = nullptr;

  float m_lineHeight = 0.0f;
  float m_ascender = 0.0f;

  std::vector<uint8_t> m_atlasPixels;
  uint32_t m_atlasWidth = 1024;
  uint32_t m_atlasHeight = 1024;
  float m_nextPackX;
  float m_nextPackY;
  uint32_t m_maxRowHeight;
  float m_textureDirty;

  void generateAtlas();

public:
  static bool initFreeType();
  FreeTypeFont() { initFreeType(); }
  ~FreeTypeFont() {
    if (m_face) {
      FT_Done_Face(m_face);
    }
  }
  bool consumeTextureDirtyBit();
  bool load(const std::string &path, uint32_t size) override;
  GlyphInfo getGlyphVariant(char32_t codepoint, uint32_t size,
                            uint8_t styleFlags) override;
  float getLineHeight() const override { return m_lineHeight; };
  float getAscender() const override { return m_ascender; };
  void *getTextureHandle() override { return m_vulkanTextureHandle; };
  GlyphInfo rasterizeAndPackGlyph(char32_t codepoint, uint32_t size,
                                  uint8_t styleFlags);
};
} // namespace ui::font
