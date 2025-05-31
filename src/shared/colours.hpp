#pragma once

#include "../plugin.hpp"

namespace DANT {

static const NVGcolor RGB_PANEL{nvgRGB(209, 243, 255)};
static const NVGcolor RGB_PANEL_DARK{nvgRGB(0, 54, 74)};
static const NVGcolor RGB_PANEL_BORDER{nvgRGB(93, 212, 255)};
static const NVGcolor RGB_PANEL_BORDER_DARK{nvgRGB(0, 115, 157)};
static const NVGcolor RGB_PANEL_TEXT{nvgRGB(0, 42, 57)};
static const NVGcolor RGB_PANEL_TEXT_DARK{nvgRGB(211, 243, 255)};

struct Colours {
  static NVGcolor getPanelColour() { return rack::settings::preferDarkPanels ? RGB_PANEL_DARK : RGB_PANEL; }

  static NVGcolor getPanelBorderColour() {
    return rack::settings::preferDarkPanels ? RGB_PANEL_BORDER_DARK : RGB_PANEL_BORDER;
  }

  static NVGcolor getTextColour() { return rack::settings::preferDarkPanels ? RGB_PANEL_TEXT_DARK : RGB_PANEL_TEXT; }
};

}  // namespace DANT
