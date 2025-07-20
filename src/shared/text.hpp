#pragma once

#include <string>
#include <vector>

#include "../plugin.hpp"
#include "colours.hpp"
#include "material-symbols.hpp"

namespace DANT {

static const std::string REGULAR_TTF{"fonts/CourierPrime-Regular.ttf"};
static const std::string BOLD_TTF{"fonts/CourierPrime-Bold.ttf"};
static const std::string SYMBOLS_TTF{"fonts/MaterialSymbolsSharp[FILL,GRAD,opsz,wght].ttf"};

struct Fonts {
  static void prepareFont(const rack::widget::Widget::DrawArgs &args, const std::string ttfFile) {
    std::shared_ptr<rack::window::Font> fontTTF =
        APP->window->loadFont(rack::asset::plugin(pluginInstance, ttfFile.c_str()));

    if (!fontTTF) {
      DEBUG("TTF [%s] failed to load", ttfFile.c_str());
    } else {
      nvgFontFaceId(args.vg, fontTTF->handle);
    }
  }

  struct DrawOptions {
    std::string ttfFile = REGULAR_TTF;
    float xpos = 0.0f;
    float ypos = 0.0f;
    NVGcolor colour = DANT::Colours::getTextColour();
    int align = NVG_ALIGN_LEFT | NVG_ALIGN_BASELINE;
    float size = 14.0f;
    float spacing = -1.0f;
    float rotate = 0.0f;

    DrawOptions() = default;
  };

  /**
   * Returns the width in pixels of the drawn text.
   */
  static float drawText(const rack::widget::Widget::DrawArgs &args, std::string text, const DrawOptions opts) {
    nvgSave(args.vg);

    prepareFont(args, opts.ttfFile);

    nvgFontSize(args.vg, opts.size);
    nvgTextAlign(args.vg, opts.align);
    nvgTextLetterSpacing(args.vg, opts.spacing);
    nvgFillColor(args.vg, opts.colour);

    nvgTranslate(args.vg, opts.xpos, opts.ypos);
    if (opts.rotate != 0.0f) {
      nvgRotate(args.vg, DANT::PI * opts.rotate);
    }

    nvgBeginPath(args.vg);
    nvgText(args.vg, 0.0f, 0.0f, text.c_str(), NULL);
    nvgFill(args.vg);

    float bounds[4];
    nvgTextBounds(args.vg, 0.0f, 0.0f, text.c_str(), NULL, bounds);

    nvgRestore(args.vg);

    //     xmax      - xmin
    return bounds[2] - bounds[0];
  }

  static float drawSymbols(const rack::widget::Widget::DrawArgs &args, const std::vector<std::string> &symbolNames,
                           DrawOptions opts, const bool spaces = false) {
    std::string symbolsToDraw;
    for (const std::string &symbolName : symbolNames) {
      auto codepointItr = DANT::SYMBOLS.find(symbolName);
      if (codepointItr != DANT::SYMBOLS.end()) {
        symbolsToDraw.append(codepointItr->second);
        if (spaces) {
          symbolsToDraw.append(" ");
        }
      }
    }

    opts.ttfFile = SYMBOLS_TTF;

    return drawText(args, symbolsToDraw, opts);
  }
};

}  // namespace DANT
