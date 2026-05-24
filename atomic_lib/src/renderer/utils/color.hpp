#pragma once
#include "math/vec.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <string_view>

namespace ui {

class AtomicColor {
public:
  math::vec4<float> rgba{0.0f, 0.0f, 0.0f, 1.0f};

  constexpr AtomicColor() = default;
  constexpr AtomicColor(float r, float g, float b, float a = 1.0f)
      : rgba{r, g, b, a} {}
  constexpr AtomicColor(const math::vec4<float> &vec) : rgba{vec} {}

  constexpr operator math::vec4<float>() const { return rgba; }

  constexpr AtomicColor operator*(float factor) const {
    return AtomicColor(std::clamp(rgba.x * factor, 0.0f, 1.0f),
                       std::clamp(rgba.y * factor, 0.0f, 1.0f),
                       std::clamp(rgba.z * factor, 0.0f, 1.0f), rgba.w);
  }

  constexpr AtomicColor operator[](uint32_t weight) const {
    float factor = 1.0f + (500.0f - static_cast<float>(weight)) * 0.0015f;
    return AtomicColor(std::clamp(rgba.x * factor, 0.0f, 1.0f),
                       std::clamp(rgba.y * factor, 0.0f, 1.0f),
                       std::clamp(rgba.z * factor, 0.0f, 1.0f), rgba.w);
  }

  constexpr AtomicColor alpha(float a) const {
    return AtomicColor(rgba.x, rgba.y, rgba.z, std::clamp(a, 0.0f, 1.0f));
  }

  static constexpr AtomicColor Hex(std::string_view hexStr) {
    if (hexStr.empty())
      return {};
    size_t offset = (hexStr[0] == '#') ? 1 : 0;

    uint32_t val = 0;
    for (size_t i = offset; i < hexStr.size(); ++i) {
      char c = hexStr[i];
      val <<= 4;
      if (c >= '0' && c <= '9')
        val |= (c - '0');
      else if (c >= 'a' && c <= 'f')
        val |= (c - 'a' + 10);
      else if (c >= 'A' && c <= 'F')
        val |= (c - 'A' + 10);
    }

    if (hexStr.size() - offset == 8) { // RRGGBBAA
      return AtomicColor(((val >> 24) & 0xFF) / 255.0f,
                         ((val >> 16) & 0xFF) / 255.0f,
                         ((val >> 8) & 0xFF) / 255.0f, (val & 0xFF) / 255.0f);
    }
    return AtomicColor(((val >> 16) & 0xFF) / 255.0f,
                       ((val >> 8) & 0xFF) / 255.0f, (val & 0xFF) / 255.0f,
                       1.0f);
  }

  static AtomicColor Hsl(float h, float s, float l, float a = 1.0f) {
    h = std::fmod(h, 360.0f) / 360.0f;
    s = std::clamp(s, 0.0f, 1.0f);
    l = std::clamp(l, 0.0f, 1.0f);

    if (s == 0.0f)
      return AtomicColor(l, l, l, a);

    auto hue2rgb = [](float p, float q, float t) {
      if (t < 0.0f)
        t += 1.0f;
      if (t > 1.0f)
        t -= 1.0f;
      if (t < 1.0f / 6.0f)
        return p + (q - p) * 6.0f * t;
      if (t < 1.0f / 2.0f)
        return q;
      if (t < 2.0f / 3.0f)
        return p + (q - p) * (2.0f / 3.0f - t) * 6.0f;
      return p;
    };

    float q = l < 0.5f ? l * (1.0f + s) : l + s - l * s;
    float p = 2.0f * l - q;

    return AtomicColor(hue2rgb(p, q, h + 1.0f / 3.0f), hue2rgb(p, q, h),
                       hue2rgb(p, q, h - 1.0f / 3.0f), a);
  }
};

// Core Palettes Global Presets
namespace Color {
// --- Pure Structural Baselines ---
inline constexpr AtomicColor transparent = {0.0f, 0.0f, 0.0f, 0.0f};
inline constexpr AtomicColor black = {0.0f, 0.0f, 0.0f, 1.0f};
inline constexpr AtomicColor white = {1.0f, 1.0f, 1.0f, 1.0f};

// --- Grays (you had none — these are essential) ---
inline constexpr AtomicColor gray50 = {0.980f, 0.980f, 0.980f, 1.0f}; // #fafafa
inline constexpr AtomicColor gray100 = {0.961f, 0.961f, 0.961f,
                                        1.0f}; // #f5f5f5
inline constexpr AtomicColor gray200 = {0.898f, 0.898f, 0.898f,
                                        1.0f}; // #e5e5e5
inline constexpr AtomicColor gray300 = {0.831f, 0.831f, 0.831f,
                                        1.0f}; // #d4d4d4
inline constexpr AtomicColor gray400 = {0.639f, 0.639f, 0.639f,
                                        1.0f}; // #a3a3a3
inline constexpr AtomicColor gray500 = {0.459f, 0.459f, 0.459f,
                                        1.0f}; // #737373
inline constexpr AtomicColor gray600 = {0.318f, 0.318f, 0.318f,
                                        1.0f}; // #525252
inline constexpr AtomicColor gray700 = {0.251f, 0.251f, 0.251f,
                                        1.0f}; // #404040
inline constexpr AtomicColor gray800 = {0.153f, 0.153f, 0.153f,
                                        1.0f}; // #262626
inline constexpr AtomicColor gray900 = {0.090f, 0.090f, 0.090f,
                                        1.0f}; // #171717
inline constexpr AtomicColor gray950 = {0.039f, 0.039f, 0.039f,
                                        1.0f}; // #0a0a0a

// --- Tailwind Core Neutral Spectrum (500 Baselines) ---
inline constexpr AtomicColor slate = {0.392f, 0.475f, 0.580f, 1.0f}; // #64748b
inline constexpr AtomicColor zinc = {0.451f, 0.451f, 0.475f, 1.0f};  // #71717a
inline constexpr AtomicColor neutral = {0.459f, 0.459f, 0.459f,
                                        1.0f};                       // #737373
inline constexpr AtomicColor stone = {0.471f, 0.459f, 0.447f, 1.0f}; // #78716c

// --- Vibrant Brand & Alert Palettes ---
inline constexpr AtomicColor red = {0.937f, 0.267f, 0.267f, 1.0f};    // #ef4444
inline constexpr AtomicColor orange = {0.976f, 0.431f, 0.129f, 1.0f}; // #f97316
inline constexpr AtomicColor amber = {0.961f, 0.608f, 0.059f, 1.0f};  // #f59e0b
inline constexpr AtomicColor yellow = {0.921f, 0.733f, 0.071f, 1.0f}; // #eab308
inline constexpr AtomicColor lime = {0.525f, 0.824f, 0.051f, 1.0f};   // #84cc16
inline constexpr AtomicColor green = {0.133f, 0.773f, 0.369f, 1.0f};  // #22c55e
inline constexpr AtomicColor emerald = {0.063f, 0.741f, 0.431f,
                                        1.0f};                        // #10b981
inline constexpr AtomicColor teal = {0.051f, 0.722f, 0.675f, 1.0f};   // #0d9488
inline constexpr AtomicColor cyan = {0.024f, 0.675f, 0.816f, 1.0f};   // #06b6d4
inline constexpr AtomicColor sky = {0.055f, 0.706f, 0.941f, 1.0f};    // #0ea5e9
inline constexpr AtomicColor blue = {0.231f, 0.510f, 0.965f, 1.0f};   // #3b82f6
inline constexpr AtomicColor indigo = {0.388f, 0.388f, 0.933f, 1.0f}; // #6366f1
inline constexpr AtomicColor violet = {0.545f, 0.357f, 0.949f, 1.0f}; // #8b5cf6
inline constexpr AtomicColor purple = {0.659f, 0.290f, 0.914f, 1.0f}; // #a855f7
inline constexpr AtomicColor fuchsia = {0.851f, 0.239f, 0.914f,
                                        1.0f};                      // #d946ef
inline constexpr AtomicColor pink = {0.925f, 0.231f, 0.522f, 1.0f}; // #ec4899
inline constexpr AtomicColor rose = {0.957f, 0.212f, 0.357f, 1.0f}; // #f43f5e

// --- Light Tints (100-level, useful for backgrounds/fills) ---
inline constexpr AtomicColor red100 = {1.000f, 0.902f, 0.902f, 1.0f}; // #fee2e2
inline constexpr AtomicColor orange100 = {1.000f, 0.929f, 0.871f,
                                          1.0f}; // #ffedd5
inline constexpr AtomicColor yellow100 = {1.000f, 0.980f, 0.859f,
                                          1.0f}; // #fef9c3
inline constexpr AtomicColor green100 = {0.863f, 0.988f, 0.910f,
                                         1.0f}; // #dcfce7
inline constexpr AtomicColor blue100 = {0.859f, 0.929f, 1.000f,
                                        1.0f}; // #dbeafe
inline constexpr AtomicColor purple100 = {0.953f, 0.918f, 1.000f,
                                          1.0f}; // #f3e8ff
inline constexpr AtomicColor pink100 = {0.988f, 0.894f, 0.945f,
                                        1.0f}; // #fce7f3

// --- Dark Shades (700-level, useful for hover states/text on light bg) ---
inline constexpr AtomicColor red700 = {0.733f, 0.114f, 0.114f, 1.0f}; // #b91c1c
inline constexpr AtomicColor orange700 = {0.757f, 0.251f, 0.027f,
                                          1.0f}; // #c2410c
inline constexpr AtomicColor yellow700 = {0.718f, 0.427f, 0.027f,
                                          1.0f}; // #b45309
inline constexpr AtomicColor green700 = {0.082f, 0.522f, 0.196f,
                                         1.0f}; // #15803d
inline constexpr AtomicColor blue700 = {0.114f, 0.306f, 0.847f,
                                        1.0f}; // #1d4ed8
inline constexpr AtomicColor purple700 = {0.427f, 0.157f, 0.769f,
                                          1.0f}; // #7e22ce
inline constexpr AtomicColor pink700 = {0.671f, 0.102f, 0.322f,
                                        1.0f}; // #be185d

// --- Semantic Aliases (use these in UI logic, not raw hues) ---
inline constexpr AtomicColor success = green;
inline constexpr AtomicColor warning = amber;
inline constexpr AtomicColor error = red;
inline constexpr AtomicColor info = sky;
inline constexpr AtomicColor successLight = green100;
inline constexpr AtomicColor warningLight = orange100;
inline constexpr AtomicColor errorLight = red100;
inline constexpr AtomicColor infoLight = blue100;
inline constexpr AtomicColor successDark = green700;
inline constexpr AtomicColor warningDark = orange700;
inline constexpr AtomicColor errorDark = red700;
inline constexpr AtomicColor infoDark = blue700;

// --- Surface / UI Shell Defaults (dark-theme biased) ---
inline constexpr AtomicColor surface = gray900;        // default bg
inline constexpr AtomicColor surfaceRaised = gray800;  // cards, panels
inline constexpr AtomicColor surfaceOverlay = gray700; // modals, tooltips
inline constexpr AtomicColor border = gray600;
inline constexpr AtomicColor borderSubtle = gray800;
inline constexpr AtomicColor textPrimary = white;
inline constexpr AtomicColor textSecondary = gray400;
inline constexpr AtomicColor textDisabled = gray600;
inline constexpr AtomicColor textInverse = gray900; // text on light bg

// --- Semi-transparent Overlays (for scrim, shadows, dimming) ---
inline constexpr AtomicColor scrim25 = {0.0f, 0.0f, 0.0f, 0.25f};
inline constexpr AtomicColor scrim50 = {0.0f, 0.0f, 0.0f, 0.50f};
inline constexpr AtomicColor scrim75 = {0.0f, 0.0f, 0.0f, 0.75f};
inline constexpr AtomicColor whiteAlpha10 = {1.0f, 1.0f, 1.0f, 0.10f};
inline constexpr AtomicColor whiteAlpha25 = {1.0f, 1.0f, 1.0f, 0.25f};
inline constexpr AtomicColor whiteAlpha50 = {1.0f, 1.0f, 1.0f, 0.50f};

} // namespace Color

inline constexpr AtomicColor operator""_hex(const char *str, size_t len) {
  return AtomicColor::Hex(std::string_view(str, len));
}

} // namespace ui
