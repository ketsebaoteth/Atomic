#pragma once

#include "renderer/font/freetype_font.hpp"

#include <memory>
#include <string>
#include <unordered_map>

namespace ui::font {

class FontCache {
public:
  static Font *get(const std::string &name, uint32_t size = 16);

private:
  static std::unordered_map<std::string, std::unique_ptr<FreeTypeFont>> s_fonts;
};

} // namespace ui::font
