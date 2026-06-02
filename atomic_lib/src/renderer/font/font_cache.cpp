#include "renderer/font/font_cache.hpp"
#include "renderer/font/font_manager.hpp"

namespace ui::font {

std::unordered_map<std::string, std::unique_ptr<FreeTypeFont>>
    FontCache::s_fonts;

Font *FontCache::get(const std::string &name, uint32_t size) {

  // FIX: include size in key (critical)
  std::string key = name + ":" + std::to_string(size);

  auto it = s_fonts.find(key);
  if (it != s_fonts.end())
    return it->second.get();

  std::string path = resolveFont(name);

  if (path.empty())
    path = resolveFont("inter");

  auto font = std::make_unique<FreeTypeFont>();

  font->load(path, size);

  Font *ptr = font.get();

  s_fonts.emplace(key, std::move(font));

  printf("here is the pointer for font %s %p\n", name.c_str(), (void *)ptr);
  return ptr;
}

} // namespace ui::font
