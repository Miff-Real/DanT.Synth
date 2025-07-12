#pragma once

#include <rack.hpp>
#include <string>

#include "../plugin.hpp"
#include "menu-slider.hpp"
#include "panel.hpp"

namespace DANT {
static const float RGB_SLIDER_WIDTH{200.0f};

struct ModuleWidget : rack::app::ModuleWidget {
  virtual std::string moduleName() { return ""; }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    DANT::drawPanel(args, this);

    rack::app::ModuleWidget::draw(args);
  }

  void drawLayer(const rack::widget::Widget::DrawArgs &args, int layer) override {
    rack::app::ModuleWidget::drawLayer(args, layer);
  }

  void appendContextMenu(rack::ui::Menu *menu) override {
    menu->addChild(new rack::ui::MenuSeparator);
    menu->addChild(rack::createSubmenuItem("Panel", "", [=](rack::ui::Menu *menu) {
      menu->addChild(rack::createMenuLabel("Bright Colour:"));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_R, &DANT::PANEL_R_B), DANT::RGB_SLIDER_WIDTH));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_G, &DANT::PANEL_G_B), DANT::RGB_SLIDER_WIDTH));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_B, &DANT::PANEL_B_B), DANT::RGB_SLIDER_WIDTH));
      menu->addChild(rack::createMenuLabel("Dark Colour:"));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_R, &DANT::PANEL_R_D), DANT::RGB_SLIDER_WIDTH));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_G, &DANT::PANEL_G_D), DANT::RGB_SLIDER_WIDTH));
      menu->addChild(new DANT::MenuSlider(new DANT::RGBValueQuantity(RGB_B, &DANT::PANEL_B_D), DANT::RGB_SLIDER_WIDTH));
    }));
  }
};
}  // namespace DANT
