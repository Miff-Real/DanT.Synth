#pragma once

#include "../plugin.hpp"
#include "colours.hpp"

namespace DANT {
static const float CIRCLE_ORIGIN_TRANSFORM{-90.0f};  // NVG uses radians, 0 rads is 3 o'clock
static const float KNOB_TO_ARC_SPACING{4.0f};
static const float KNOB_NOTCH_WIDTH{1.5f};
static const float KNOB_ARC_WIDTH{2.5f};

static NVGpaint knobTexture(const rack::widget::Widget::DrawArgs &args, const float width, const float height) {
  std::shared_ptr<rack::window::Image> metalGradTex =
      APP->window->loadImage(rack::asset::plugin(pluginInstance, "res/metal-grad.png"));
  return nvgImagePattern(args.vg, 0.0f, 0.0f, width, height, 0.0f, metalGradTex->handle, 1.0f);
}

enum KnobViz {
  NONE,     // No visualisation, just a plain knob
  NOTCHES,  // A line with notches
  UNIARC,   // A unipolar arc, optional CV offset & attenuverter
  BIPARC    // A bipolar arc, optional CV offset & attenuverter
};

struct Knob : rack::componentlibrary::RoundKnob {
  const double angle{0.85};     // how far the knob rotates, 1 = 360 degrees
  DANT::KnobViz vizType{NONE};  // knob visualisation
  int numNotches{5};            // number of notches to draw on the visualisation
  int inputId{-1};              // optional input ID for parameter CV
  float cvScaler{1.0f};         // multiplier for the raw CV value
  int cvAttId{-1};              // optional param ID for CV input attenuverter

  struct ArcData {
    float knobWidth;       // for knob BG image
    float knobHeight;      // for knob BG image
    float centreX;         // centre for arcs & circles
    float centreY;         // centre for arcs & circles
    float arcRadius;       // knob radius plus arc spacing
    float rangeDeg;        // distance from left extent to right extent in degrees
    float halfRangeDeg;    // distance from 12 o'clock to right extent in degrees
    float leftExtentDeg;   // left extent location in degrees
    float leftExtentRad;   // left extent location in radians
    float rightExtentDeg;  // right extent location in degrees
    float rightExtentRad;  // right extent location in radians
  };

  ArcData arcData;

  Knob() {
    this->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/knob-fg.svg")));
    this->bg->setSvg(APP->window->loadSvg(rack::asset::plugin(pluginInstance, "res/knob-bg.svg")));
    this->shadow->opacity = 0.0f;
    this->minAngle = -this->angle * DANT::PI;
    this->maxAngle = this->angle * DANT::PI;
    this->arcData.knobWidth = this->box.size.x;
    this->arcData.knobHeight = this->box.size.y;
    this->arcData.centreX = this->arcData.knobWidth * 0.5f;
    this->arcData.centreY = this->arcData.knobHeight * 0.5f;
    this->arcData.arcRadius = (this->arcData.knobWidth + KNOB_TO_ARC_SPACING) * 0.5f;
    this->arcData.rangeDeg = 360.0f * this->angle;
    this->arcData.halfRangeDeg = 180.0f * this->angle;
    this->arcData.leftExtentDeg = CIRCLE_ORIGIN_TRANSFORM - this->arcData.halfRangeDeg;
    this->arcData.leftExtentRad = nvgDegToRad(this->arcData.leftExtentDeg);
    this->arcData.rightExtentDeg = CIRCLE_ORIGIN_TRANSFORM + this->arcData.halfRangeDeg;
    this->arcData.rightExtentRad = nvgDegToRad(this->arcData.rightExtentDeg);
  }

  float getParamValue(const bool bip = false) {
    if (this->module) {
      float v{this->module->params[this->paramId].getValue()};
      rack::engine::ParamQuantity *pq{this->module->getParamQuantity(this->paramId)};
      return rack::math::rescale(v, pq->getMinValue(), pq->getMaxValue(), bip ? -1.0f : 0.0f, 1.0f);
    }
    return bip ? 0.0f : 0.5f;
  }

  float getCvValue(const bool bip = false) {
    if (this->inputId > -1 && this->module) {
      float v{this->module->inputs[this->inputId].getNormalVoltage(0.0f)};
      float minCv{bip ? -10.0f : 0.0f};
      float maxCv{10.0f};
      return rack::math::rescale(v, minCv * this->cvScaler, maxCv * this->cvScaler, 0.0f, 1.0f);
    }
    return 0.0f;
  }

  float getCvAttValue() {
    if (this->cvAttId > -1 && this->module) {
      return this->module->params[this->cvAttId].getValue();
    }
    return 1.0f;
  }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    nvgSave(args.vg);

    drawKnobBG(args);

    if (this->vizType != NONE) {
      switch (this->vizType) {
        case NONE:
          break;
        case NOTCHES:
          drawNotchesViz(args);
          break;
        case UNIARC:
          drawArcViz(args);
          break;
        case BIPARC:
          drawArcViz(args);
          break;
      }
    }

    nvgRestore(args.vg);

    rack::componentlibrary::RoundKnob::draw(args);
  }

  void drawLayer(const rack::widget::Widget::DrawArgs &args, int layer) override {
    if (layer == 1) {
      if (this->vizType != NONE) {
        nvgSave(args.vg);
        switch (this->vizType) {
          case NONE:
            break;
          case NOTCHES:
            break;
          case UNIARC:
            drawArcUniValue(args);
            break;
          case BIPARC:
            drawArcBipValue(args);
            break;
        }
        nvgRestore(args.vg);
      }
    }
    rack::componentlibrary::RoundKnob::drawLayer(args, layer);
  }

  void drawKnobBG(const rack::widget::Widget::DrawArgs &args) {
    nvgFillPaint(args.vg, knobTexture(args, this->arcData.knobWidth, this->arcData.knobHeight));

    nvgBeginPath(args.vg);
    nvgCircle(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.centreX);
    nvgFill(args.vg);
  }

  void drawNotchesViz(const rack::widget::Widget::DrawArgs &args) {
    nvgStrokeWidth(args.vg, KNOB_NOTCH_WIDTH);
    nvgStrokeColor(args.vg, DANT::Colours::getTextColour());

    nvgBeginPath(args.vg);
    nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius, this->arcData.leftExtentRad,
           this->arcData.rightExtentRad, NVG_CW);
    nvgStroke(args.vg);

    drawNotches(args);
  }

  void drawArcViz(const rack::widget::Widget::DrawArgs &args) {
    nvgStrokeWidth(args.vg, KNOB_ARC_WIDTH);
    NVGcolor arcColour = DANT::Colours::getTextColour();
    arcColour.a = 0.8f;
    nvgStrokeColor(args.vg, arcColour);

    nvgBeginPath(args.vg);
    nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius, this->arcData.leftExtentRad,
           this->arcData.rightExtentRad, NVG_CW);
    nvgStroke(args.vg);

    drawNotches(args);
  }

  void drawNotches(const rack::widget::Widget::DrawArgs &args) {
    if (this->numNotches < 2) {
      return;
    }

    // calculate distance between notches in degrees
    const float segmentDeg{static_cast<float>(360.0 * this->angle) / static_cast<float>(this->numNotches - 1)};
    // half width of a notch in degrees
    const float hnwDeg{3.0f};

    for (int n{0}; n < this->numNotches; ++n) {
      float notchCentreDeg{this->arcData.leftExtentDeg + (segmentDeg * n)};
      float notchStartRad{nvgDegToRad(notchCentreDeg - hnwDeg)};
      float notchEndRad{nvgDegToRad(notchCentreDeg + hnwDeg)};

      nvgStrokeWidth(args.vg, KNOB_NOTCH_WIDTH * 2.5f);

      nvgBeginPath(args.vg);
      nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius, notchStartRad, notchEndRad,
             NVG_CW);
      nvgStroke(args.vg);
    }
  }

  void drawArcUniValue(const rack::widget::Widget::DrawArgs &args) {
    float paramExtentDeg{
        rack::math::clampSafe(this->arcData.leftExtentDeg + (this->arcData.rangeDeg * this->getParamValue()),
                              this->arcData.leftExtentDeg, this->arcData.rightExtentDeg)};

    nvgStrokeWidth(args.vg, KNOB_ARC_WIDTH - 1.5f);
    nvgStrokeColor(args.vg, DANT::Colours::getCvGreenColour());

    nvgBeginPath(args.vg);
    nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius, this->arcData.leftExtentRad,
           nvgDegToRad(paramExtentDeg), NVG_CW);
    nvgStroke(args.vg);

    float cv{this->getCvValue() * this->getCvAttValue()};
    if (cv != 0.0f) {
      float cvExtentDeg{rack::math::clampSafe(paramExtentDeg + (this->arcData.rangeDeg * cv),
                                              this->arcData.leftExtentDeg, this->arcData.rightExtentDeg)};
      bool cvIsNegative{cvExtentDeg < paramExtentDeg};

      nvgStrokeColor(args.vg, cvIsNegative ? RGB_CV_RED : RGB_CV_YELLOW);

      nvgBeginPath(args.vg);
      nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius,
             nvgDegToRad(paramExtentDeg), nvgDegToRad(cvExtentDeg), cvIsNegative ? NVG_CCW : NVG_CW);
      nvgStroke(args.vg);
    }
  }

  void drawArcBipValue(const rack::widget::Widget::DrawArgs &args) {
    const bool bip{true};
    float pv{this->getParamValue(bip)};
    bool paramIsNegative{pv < 0.0f};

    nvgStrokeWidth(args.vg, KNOB_ARC_WIDTH - 1.5f);
    nvgStrokeColor(args.vg, paramIsNegative ? RGB_CV_RED : DANT::Colours::getCvGreenColour());

    float arcSizeDeg;
    if (paramIsNegative) {
      arcSizeDeg = rack::math::clampSafe(CIRCLE_ORIGIN_TRANSFORM + (this->arcData.halfRangeDeg * pv),
                                         this->arcData.leftExtentDeg, CIRCLE_ORIGIN_TRANSFORM);
    } else {
      arcSizeDeg = rack::math::clampSafe(CIRCLE_ORIGIN_TRANSFORM + (this->arcData.halfRangeDeg * pv),
                                         CIRCLE_ORIGIN_TRANSFORM, this->arcData.rightExtentDeg);
    }

    nvgBeginPath(args.vg);
    nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius,
           nvgDegToRad(CIRCLE_ORIGIN_TRANSFORM), nvgDegToRad(arcSizeDeg), paramIsNegative ? NVG_CCW : NVG_CW);
    nvgStroke(args.vg);

    float cv{this->getCvValue() * this->getCvAttValue()};
    if (cv != 0.0f) {
      float cvExtentDeg{rack::math::clampSafe(arcSizeDeg + (this->arcData.rangeDeg * cv), this->arcData.leftExtentDeg,
                                              this->arcData.rightExtentDeg)};
      bool cvIsNegative{cvExtentDeg < arcSizeDeg};

      nvgStrokeColor(args.vg, RGB_CV_YELLOW);

      nvgBeginPath(args.vg);
      nvgArc(args.vg, this->arcData.centreX, this->arcData.centreY, this->arcData.arcRadius, nvgDegToRad(arcSizeDeg),
             nvgDegToRad(cvExtentDeg), cvIsNegative ? NVG_CCW : NVG_CW);
      nvgStroke(args.vg);
    }
  }
};
}  // namespace DANT
