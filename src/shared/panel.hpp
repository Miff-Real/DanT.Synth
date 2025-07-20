#pragma once

#include "../plugin.hpp"
#include "colours.hpp"
#include "text.hpp"

namespace DANT {

/**
 * Stroke widths for easy calculations.
 */
static const float strk{1.0f};
static const float hlfStrk{strk * 0.5f};
static const float dblStrk{strk * 2.0f};

const std::vector<std::string> logoSymbol = {"headphones"};

/**
 * Draw a module panel.
 * Respects rack dark panel setting.
 */
template <typename W>
void drawPanel(const rack::widget::Widget::DrawArgs &args, W *widget) {
  nvgSave(args.vg);

  nvgFillColor(args.vg, DANT::Colours::getPanelBorderColour());
  // full size first, this will be the border
  nvgBeginPath(args.vg);
  nvgRect(args.vg, 0.0f, 0.0f, widget->box.size.x, widget->box.size.y);
  nvgFill(args.vg);

  nvgFillColor(args.vg, DANT::Colours::getPanelColour());
  // size reduced by border width, on top of previous rect
  nvgBeginPath(args.vg);
  nvgRect(args.vg, strk, strk, widget->box.size.x - dblStrk, widget->box.size.y - dblStrk);
  nvgFill(args.vg);

  // panels have the DanT logo and name top left
  DANT::Fonts::DrawOptions opts;
  opts.ttfFile = DANT::BOLD_TTF;
  opts.xpos = 1.5f;
  opts.ypos = 0.0f;
  opts.align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP;
  float logoWidth = DANT::Fonts::drawText(args, "DanT", opts);
  opts.ttfFile = DANT::REGULAR_TTF;
  opts.xpos += logoWidth + 1.0f;
  DANT::Fonts::drawText(args, widget->moduleName().c_str(), opts);

  nvgRestore(args.vg);
}

}  // namespace DANT
