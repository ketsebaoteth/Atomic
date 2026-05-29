#include "layout/elements/div.hpp"
#include "layout/elements/text.hpp"
#include "renderer/style.hpp"
#include "renderer/utils/color.hpp"
#include "windowing/interface.hpp"
#include "windowing/linux/window_sdl.hpp"
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace ui;

// ============================================================================
// SHADCN BLACKOUT CONFIGURATION HELPER (STABILIZED DEEP CONTRAST SPECS)
// ============================================================================
namespace StylePreset {
inline ui::AtomicColor Background() {
  return ui::Color::gray950;
} // #0a0a0a - True rich black canvas
inline ui::AtomicColor CardBg() {
  return ui::Color::gray900;
} // #171717 - Deep structural container baseline
inline ui::AtomicColor Border() {
  return ui::Color::gray800;
} // #262626 - High-contrast separation grid line
inline ui::AtomicColor MutedText() {
  return ui::Color::gray400;
} // Clean auxiliary textual labels
inline ui::AtomicColor AccentTeal() {
  return ui::Color::teal[400];
} // High-visibility interactive accent glow
} // namespace StylePreset

// ============================================================================
// SOLIDIFIED LAYER COMPONENT PROTOTYPES (ANIMATION CONSTRICTOR ARCHITECTURE)
// ============================================================================

// Minimalistic Status Badge (Structural radius locked to prevent pipeline shape
// wobble)
auto AnimatedBadge(const std::string &statusText, float interpolationFactor) {
  // Rigid geometric boundary ensures stability under alpha composition passes
  const float stableRadius = 6.0f;
  float dynamicPaddingHorizontal =
      10.0f + (18.0f - 10.0f) * interpolationFactor;

  return Div(ui::styleConfig()
                 .SetSize({ui::SizeFit{}, ui::SizeFit{}})
                 .SetBGColor(static_cast<math::vec4<float>>(ui::Color::gray800))
                 .SetPadding({6.0f, dynamicPaddingHorizontal, 6.0f,
                              dynamicPaddingHorizontal})
                 .SetRadius(CornerRadius::all(stableRadius)),
             Text(ui::styleConfig()
                      .SetBGColor(static_cast<math::vec4<float>>(
                          StylePreset::AccentTeal()))
                      .SetFontSize(12),
                  statusText));
}

// Interactive Sidebar Nav Element (Pure color/opacity transitions only)
auto ShadcnSidebarRoute(const std::string &label, bool isActive) {
  ui::AtomicColor selectedColor =
      isActive ? ui::Color::gray800 : ui::Color::transparent;
  ui::AtomicColor textColor =
      isActive ? ui::Color::white : StylePreset::MutedText();

  return Div(ui::styleConfig()
                 .SetSize({ui::SizeFill{}, ui::SizeFit{}})
                 .SetBGColor(static_cast<math::vec4<float>>(selectedColor))
                 .SetPadding({8.0f, 12.0f, 8.0f, 12.0f})
                 .SetRadius(CornerRadius::all(6.0f)),
             Text(ui::styleConfig()
                      .SetBGColor(static_cast<math::vec4<float>>(textColor))
                      .SetFontSize(14),
                  label));
}

// Modular Adaptive Grid Card (Isolates active content metrics from baseline
// geometry)
auto MorphingMatrixCard(const std::string &label,
                        const std::string &primaryValue, float waveVal,
                        ui::AtomicColor livePulseColor) {
  float calculatedHeight = 120.0f + (35.0f * waveVal);
  const float stableBorderRadius =
      8.0f; // Locked down to preserve layout depth boundaries

  return Div(
      ui::styleConfig()
          .SetSize({fill, calculatedHeight})
          .SetBGColor(static_cast<math::vec4<float>>(StylePreset::CardBg()))
          .SetPadding(EdgeInsets::all(14.0f))
          .SetRadius(CornerRadius::all(stableBorderRadius))
          .SetFlexDirection(FlexDirection::Column)
          .SetGap({5.0f, 5.0f}),

      Text(ui::styleConfig()
               .SetBGColor(
                   static_cast<math::vec4<float>>(StylePreset::MutedText()))
               .SetFontSize(12),
           label),
      Text(ui::styleConfig()
               .SetBGColor(static_cast<math::vec4<float>>(livePulseColor))
               .SetFontSize(28),
           primaryValue));
}

// Execution Stream Log Rows
auto ShadcnLogStreamRow(const std::string &timestamp, const std::string &msg,
                        ui::AtomicColor flagColor) {
  return Div(
      ui::styleConfig()
          .SetSize({ui::SizeFill{}, ui::SizeFit{}})
          .SetFlexDirection(FlexDirection::Row)
          .SetGap({6.0f, 0.0f}),
      Text(ui::styleConfig()
               .SetBGColor(static_cast<math::vec4<float>>(ui::Color::gray600))
               .SetFontSize(13),
           "[" + timestamp + "]"),
      Div(ui::styleConfig()
              .SetSize({4.0f, 14.0f})
              .SetBGColor(static_cast<math::vec4<float>>(flagColor))
              .SetRadius(CornerRadius::all(2.0f))),
      Text(ui::styleConfig()
               .SetBGColor(static_cast<math::vec4<float>>(ui::Color::gray300))
               .SetFontSize(13),
           msg));
}

auto GradientTestUI() {
  return Div(
      ui::styleConfig()
          .SetSize({fill, fill})
          .SetBGColor({0, 0, 0, 0})
          .SetPadding(EdgeInsets::all(0.0f))
          .SetFlexDirection(FlexDirection::Column)
          .SetGap({12.0f, 12.0f}),

      // ---------------------------
      // LINEAR GRADIENT BOX
      // ---------------------------
      Div(ui::styleConfig().SetSize({fill, 80.0f}).SetBGColor({0, 0, 0, 0})),

      Div(ui::styleConfig()
              .SetSize({fill, 120.0f})
              .SetGradientType(GradientType::Linear)
              .SetLinearGradDirection(45.0f,
                                      GradientDirectionUnit::Deg) // 45 degrees
              .SetGradientStops({{0.0f, {1, 1, 1, 1}},
                                 {0.5f, {0.5, 0.5, 0.5f, 1}},
                                 {1.0f, {0, 0, 0, 1}}})
              //.SetBGColor({1, 0, 1, 1})
              .SetRadius(CornerRadius::all(12.0f))
              .SetPadding(EdgeInsets::all(10.0f))
              .SetOpacity(1.0f),

          Text(ui::styleConfig().SetFontSize(20).SetBGColor({1, 1, 1, 1}),
               "Linear Gradient")),

      // ---------------------------
      // RADIAL GRADIENT BOX
      // ---------------------------
      Div(ui::styleConfig()
              .SetSize({fill, 160.0f})
              .SetGradientType(GradientType::Radial)
              .SetRadialGradCenter({0.3f, 0.3f})
              .SetGradientStops({{0.0f, {0, 0, 0, 1}}, {0.5f, {1, 1, 1, 1}}})
              .SetRadius(CornerRadius::all(16.0f))
              .SetOpacity(0.9f),

          Text(ui::styleConfig().SetFontSize(18).SetBGColor(
                   {0.9f, 0.9f, 0.9f, 1}),
               "Radial Gradient Layer")));
}

// ============================================================================
// CORE HIERARCHY TREE BUILDER PASS (STATIC SHELL EXCLUSION)
// ============================================================================
auto BuildShowcaseCanvas(float globalWave, ui::AtomicColor liveAccent) {
  // Enforcing strict compile-time architectural separation for structural
  // shells
  const auto bg = StylePreset::Background();
  const auto cardBg = StylePreset::CardBg();
  const auto border = StylePreset::Border();

  return Div(
      ui::styleConfig()
          .SetSize({fill, fill})
          .SetBGColor(static_cast<math::vec4<float>>(bg))
          .SetPadding(EdgeInsets::all(16.0f))
          .SetFlexDirection(FlexDirection::Row)
          .SetGap({12.0f, 0.0f}),

      // 1. Left Rail Navigation (Rigid Static Grounding)
      Div(ui::styleConfig()
              .SetSize({220.0f, fill})
              .SetBGColor(static_cast<math::vec4<float>>(cardBg))
              .SetPadding(EdgeInsets::all(16.0f))
              .SetRadius(CornerRadius::all(8.0f))
              .SetFlexDirection(FlexDirection::Column)
              .SetGap({6.0f, 6.0f}),

          Text(ui::styleConfig()
                   .SetBGColor(
                       static_cast<math::vec4<float>>(ui::Color::gray500))
                   .SetFontSize(18),
               "ATOMIC_DSP"),

          Div(ui::styleConfig()
                  .SetSize({ui::SizeFill{}, 1.0f})
                  .SetBGColor(static_cast<math::vec4<float>>(border))),

          ShadcnSidebarRoute("System Status", true),
          ShadcnSidebarRoute("Graph Engines", false),
          ShadcnSidebarRoute("Cluster Node", false)),

      // 2. Workspace Area
      Div(ui::styleConfig()
              .SetSize({fill, fill})
              .SetBGColor(
                  static_cast<math::vec4<float>>(ui::Color::transparent))
              .SetFlexDirection(FlexDirection::Column)
              .SetGap({12.0f, 12.0f}),

          // Top Command Bar
          Div(ui::styleConfig()
                  .SetSize({ui::SizeFill{}, ui::SizeFit{}})
                  .SetBGColor(static_cast<math::vec4<float>>(cardBg))
                  .SetPadding({12.0f, 16.0f, 12.0f, 16.0f})
                  .SetRadius(CornerRadius::all(8.0f))
                  .SetFlexDirection(FlexDirection::Row)
                  .SetGap({0.0f, 0.0f}),
              Text(ui::styleConfig()
                       .SetBGColor(
                           static_cast<math::vec4<float>>(ui::Color::gray100))
                       .SetFontSize(20),
                   "Topology Overview Engine Pipeline"),
              AnimatedBadge("CORE REALTIME COMPILER ACTIVE", globalWave)),

          // 3. Isolated Morphing Grid Row
          Div(ui::styleConfig()
                  .SetSize({ui::SizeFill{}, ui::SizeFit{}})
                  .SetBGColor(
                      static_cast<math::vec4<float>>(ui::Color::transparent))
                  .SetFlexDirection(FlexDirection::Row)
                  .SetGap({10.0f, 0.0f}),

              MorphingMatrixCard("CORE RUNTIME METRIC_A", "0.24 ms", globalWave,
                                 liveAccent),
              MorphingMatrixCard("CORE RUNTIME METRIC_B", "98.4 %",
                                 (1.0f - globalWave),
                                 StylePreset::AccentTeal()),
              MorphingMatrixCard("CORE RUNTIME METRIC_C", "1024 Hz", globalWave,
                                 ui::Color::fuchsia[400])),

          // 4. Console Logs Window (Static Shell containing dynamic signal
          // flags)
          Div(ui::styleConfig()
                  .SetSize({ui::SizeFill{}, ui::SizeFill{}})
                  .SetBGColor(static_cast<math::vec4<float>>(cardBg))
                  .SetPadding(EdgeInsets::all(16.0f))
                  .SetRadius(CornerRadius::all(8.0f))
                  .SetFlexDirection(FlexDirection::Column)
                  .SetGap({5.0f, 5.0f}),

              Text(ui::styleConfig()
                       .SetBGColor(
                           static_cast<math::vec4<float>>(ui::Color::gray200))
                       .SetFontSize(14),
                   "Live Hardware Virtualization Logs Console Stream:"),

              Div(ui::styleConfig()
                      .SetSize({ui::SizeFill{}, 1.0f})
                      .SetBGColor(static_cast<math::vec4<float>>(border))),

              ShadcnLogStreamRow("14:22:01",
                                 "Allocating matrix graphics pipelines across "
                                 "engine device queues...",
                                 ui::Color::sky[400]),
              ShadcnLogStreamRow("14:22:01",
                                 "Synchronizing thread context maps within KCC "
                                 "processor pools...",
                                 liveAccent),
              ShadcnLogStreamRow("14:22:02",
                                 "Structural node canvas elements successfully "
                                 "drawn without memory hitches.",
                                 StylePreset::AccentTeal()),
              GradientTestUI())));
}

// ============================================================================
// MAIN RUNTIME APPLICATION EXECUTION ENTRY
// ============================================================================
int main() {
  try {
    ui::WindowConfig config{"Atomic Engine - Developer Dashboard", 1200, 720,
                            100, 100};
    auto window = ui::SDLWindow::Create(config);

    auto startTime = std::chrono::high_resolution_clock::now();

    while (!window->should_close()) {
      window->poll_events();

      auto currentTime = std::chrono::high_resolution_clock::now();
      float elapsedTime =
          std::chrono::duration<float>(currentTime - startTime).count();

      float globalWave = 0.5f + 0.5f * std::sin(elapsedTime * 2.8f);

      math::vec4<float> baseVec =
          static_cast<math::vec4<float>>(ui::Color::gray400);
      math::vec4<float> highGlowVec =
          static_cast<math::vec4<float>>(ui::Color::teal[400]);

      math::vec4<float> interpolatedVec = {
          baseVec.x + (highGlowVec.x - baseVec.x) * globalWave,
          baseVec.y + (highGlowVec.y - baseVec.y) * globalWave,
          baseVec.z + (highGlowVec.z - baseVec.z) * globalWave, 1.0f};

      ui::AtomicColor frameComputedAccentColor(interpolatedVec);

      auto activeFrameGraphNode =
          BuildShowcaseCanvas(globalWave, frameComputedAccentColor);
      window->SetRootNode(std::move(activeFrameGraphNode));

      window->render();
    }
  } catch (const std::exception &e) {
    std::cerr << "Engine Crash: " << e.what() << std::endl;
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}
