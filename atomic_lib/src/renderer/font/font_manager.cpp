#include "renderer/font/font_manager.hpp"

#include <fontconfig/fontconfig.h>

namespace ui::font {

std::string resolveFont(const std::string &name) {
  FcInit();

  FcPattern *pattern =
      FcNameParse(reinterpret_cast<const FcChar8 *>(name.c_str()));

  FcConfigSubstitute(nullptr, pattern, FcMatchPattern);
  FcDefaultSubstitute(pattern);

  FcResult result;
  FcPattern *match = FcFontMatch(nullptr, pattern, &result);

  FcPatternDestroy(pattern);

  if (!match)
    return "";

  FcChar8 *file = nullptr;

  if (FcPatternGetString(match, FC_FILE, 0, &file) != FcResultMatch) {
    FcPatternDestroy(match);
    return "";
  }

  std::string path = reinterpret_cast<char *>(file);

  FcPatternDestroy(match);

  return path;
}

} // namespace ui::font
