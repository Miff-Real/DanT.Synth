#pragma once

#include "../plugin.hpp"

namespace DANT {
struct Trimpot : rack::componentlibrary::RoundKnob {
  const double angle{0.85};  // how far the knob rotates, 1 = 360 degrees

  Trimpot() {
    this->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/trimpot-fg.svg")));
    this->bg->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/trimpot-bg.svg")));
    this->shadow->opacity = 0.0f;
    this->minAngle = -this->angle * DANT::PI;
    this->maxAngle = this->angle * DANT::PI;
  }
};
}  // namespace DANT
