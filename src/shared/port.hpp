#pragma once

#include "../plugin.hpp"
#include "colours.hpp"

namespace DANT {
static const float PORT_CIRCLE_STROKE{1.5f};
static const float PORT_CIRCLE_SIZE{12.5f};

struct Port : rack::app::SvgPort {
  bool isOutput{false};

  Port() {
    this->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/port.svg")));
    this->shadow->opacity = 0.0f;
  }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    if (this->isOutput) {
      drawOutputBG(args);
    }

    rack::app::SvgPort::draw(args);
  }

  void drawOutputBG(const rack::widget::Widget::DrawArgs &args) {
    nvgSave(args.vg);

    nvgStrokeWidth(args.vg, PORT_CIRCLE_STROKE);
    nvgStrokeColor(args.vg, DANT::Colours::getTextColour());

    nvgBeginPath(args.vg);
    nvgCircle(args.vg, this->box.getWidth() * 0.5f, this->box.getHeight() * 0.5f, PORT_CIRCLE_SIZE);
    nvgStroke(args.vg);

    nvgRestore(args.vg);
  }
};
}  // namespace DANT
