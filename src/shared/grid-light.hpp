#pragma once

#include "../plugin.hpp"
#include "colours.hpp"

namespace DANT {
static const float GRID_EDGE{1.0f};
static const float GRID_CELL{2.0f};
static const float GRID_SPACING{1.0f};

struct GridLight : rack::widget::SvgWidget {
  const int *numChannels{nullptr};
  const rack::simd::float_4 *channelValues{nullptr};  // pointer to array of simd floats

  float minChannelValue{-5.0f};  // full intensity negative light at this value
  float maxChannelValue{5.0f};   // full intensity positive light at this value

  NVGcolor negativeColour{DANT::RGB_CV_RED};
  NVGcolor positiveColour{DANT::RGB_CV_GREEN};

  GridLight() { this->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/grid-bg.svg"))); }

  void fullMode() {  // any signals
    this->minChannelValue = -10.0f;
    this->maxChannelValue = 10.0f;
  }

  void uniMode() {  // unipolar signals
    this->minChannelValue = 0.0f;
    this->maxChannelValue = 10.0f;
  }

  void bipMode() {  // bipolar or audio signals
    this->minChannelValue = -5.0f;
    this->maxChannelValue = 5.0f;
  }

  void oneMode() {  // small variance signals
    this->minChannelValue = -1.0f;
    this->maxChannelValue = 1.0f;
  }

  int getNumChannels() {
    if (this->numChannels == nullptr) {
      return DANT::CHANS;
    } else {
      return *this->numChannels;
    }
  }

  float getChannelValue(const int chanIndex) {
    if (this->channelValues == nullptr) {
      return this->maxChannelValue;
    } else {
      return this->channelValues[DANT::SIMD_I[chanIndex]][DANT::SIMD_J[chanIndex]];
    }
  }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    rack::widget::SvgWidget::draw(args);
    const bool placeholders{true};
    nvgSave(args.vg);
    channelLoop(args, placeholders);
    nvgRestore(args.vg);
  }

  void drawLayer(const rack::widget::Widget::DrawArgs &args, int layer) override {
    if (layer == 1) {
      nvgSave(args.vg);
      channelLoop(args);
      nvgRestore(args.vg);
    }
  }

  void channelLoop(const rack::widget::Widget::DrawArgs &args, const bool placeholders = false) {
    int channel{0};
    NVGcolor colour{DANT::RGB_UNLIT};
    for (int x{0}; x < DANT::SIMD; ++x) {
      for (int y{0}; y < DANT::SIMD; ++y) {
        channel = (x * DANT::SIMD) + (y + 1);
        if (channel <= getNumChannels()) {
          if (!placeholders) {
            float chanValue{getChannelValue(channel - 1)};
            if (chanValue != 0.0f) {
              colour = chanValue < 0.0f ? this->negativeColour : this->positiveColour;
              colour.a = chanValue < 0.0f ? (chanValue / this->minChannelValue) : (chanValue / this->maxChannelValue);
            }
          }
          drawChanSquare(args,
                         rack::math::Vec(GRID_EDGE + ((x + 1) * GRID_SPACING) + (x * GRID_CELL),
                                         GRID_EDGE + ((y + 1) * GRID_SPACING) + (y * GRID_CELL)),
                         colour);
        }
      }
    }
  }

  void drawChanSquare(const rack::widget::Widget::DrawArgs &args, const rack::math::Vec pos, const NVGcolor colour) {
    nvgFillColor(args.vg, colour);

    nvgBeginPath(args.vg);
    nvgRect(args.vg, pos.x, pos.y, 2.0f, 2.0f);
    nvgFill(args.vg);
  }
};
}  // namespace DANT
