#pragma once

#include <rack.hpp>
#include <string>

#include "panel.hpp"

namespace DANT {
struct ModuleWidget : rack::app::ModuleWidget {
  virtual std::string moduleName() { return ""; }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    DANT::drawPanel(args, this);

    rack::widget::Widget::draw(args);
  }
};
}  // namespace DANT
