#pragma once

#include <algorithm>  // std::min, std::max
#include <cmath>      // std::fmod

#include "../plugin.hpp"

namespace DANT {

static const float PANEL_BORDER_CONTRAST{0.19f};
static const float PANEL_TEXT_CONTRAST{0.8f};

static const NVGcolor RGB_CV_GREEN{nvgRGB(154, 213, 66)};
static const NVGcolor RGB_CV_GREEN_DARK{nvgRGB(0, 165, 81)};
static const NVGcolor RGB_CV_RED{nvgRGB(209, 26, 72)};
static const NVGcolor RGB_CV_YELLOW{nvgRGB(255, 201, 0)};

static const NVGcolor RGB_UNLIT{nvgRGB(63, 63, 63)};

struct Colours {
  struct HSLcolour {
    float h;
    float s;
    float l;
  };

  static HSLcolour rgbToHsl(const NVGcolor fromColour) {
    HSLcolour result;

    const float rgbMax{std::max({fromColour.r, fromColour.g, fromColour.b})};
    const float rgbMin{std::min({fromColour.r, fromColour.g, fromColour.b})};
    const float chroma{rgbMax - rgbMin};

    result.l = (rgbMax + rgbMin) / 2.0f;

    if (chroma == 0.0f) {
      result.h = 0.0f;
      result.s = 0.0f;
    } else {
      result.s = (result.l > 0.5f) ? chroma / (2.0f - rgbMax - rgbMin) : chroma / (rgbMax + rgbMin);

      if (rgbMax == fromColour.r) {
        result.h = std::fmod((fromColour.g - fromColour.b) / chroma, 6.0f);
      } else if (rgbMax == fromColour.g) {
        result.h = ((fromColour.b - fromColour.r) / chroma) + 2.0f;
      } else {  // rgbMax == fromColour.b
        result.h = ((fromColour.r - fromColour.g) / chroma) + 4.0f;
      }
      result.h /= 6.0f;
    }
    if (result.h < 0.0f) {
      result.h += 1.0f;
    }

    return result;
  }

  static NVGcolor getBrightColour() {
    return nvgRGB(static_cast<unsigned char>(DANT::PANEL_R_B), static_cast<unsigned char>(DANT::PANEL_G_B),
                  static_cast<unsigned char>(DANT::PANEL_B_B));
  }

  static NVGcolor getDarkColour() {
    return nvgRGB(static_cast<unsigned char>(DANT::PANEL_R_D), static_cast<unsigned char>(DANT::PANEL_G_D),
                  static_cast<unsigned char>(DANT::PANEL_B_D));
  }

  static NVGcolor getContrast(const NVGcolor fromColour, const float amount) {
    HSLcolour components{rgbToHsl(fromColour)};
    components.l = std::min(std::max(components.l + amount, 0.0f), 1.0f);
    return nvgHSL(components.h, components.s, components.l);
  }

  static NVGcolor getPanelColour() { return rack::settings::preferDarkPanels ? getDarkColour() : getBrightColour(); }

  static NVGcolor getPanelBorderColour() {
    return rack::settings::preferDarkPanels ? getContrast(getDarkColour(), PANEL_BORDER_CONTRAST)
                                            : getContrast(getBrightColour(), -PANEL_BORDER_CONTRAST);
  }

  static NVGcolor getTextColour() {
    return rack::settings::preferDarkPanels ? getContrast(getDarkColour(), PANEL_TEXT_CONTRAST)
                                            : getContrast(getBrightColour(), -PANEL_TEXT_CONTRAST);
  }

  static NVGcolor getCvGreenColour() { return rack::settings::preferDarkPanels ? RGB_CV_GREEN_DARK : RGB_CV_GREEN; }
};

}  // namespace DANT
