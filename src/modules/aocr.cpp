#include <algorithm>  // std::copy std::fill
#include <string>

#include "../dsp/att-off-clip-rect.hpp"
#include "../plugin.hpp"
#include "../shared/grid-light.hpp"
#include "../shared/knob.hpp"
#include "../shared/module-widget.hpp"
#include "../shared/port.hpp"
#include "../shared/trimpot.hpp"

/**
 * Constant values.
 */
const int HP{5};

/**
 * Quantity for knob tooltip value display.
 */
struct OpOrderQuantity : rack::ParamQuantity {
  std::string getDisplayValueString() override {
    switch (static_cast<int>(getDisplayValue())) {
      case 0:
        return "AOCR";
        break;
      case 1:
        return "ACOR";
        break;
      case 2:
        return "ACRO";
        break;
      case 3:
        return "OACR";
        break;
      case 4:
        return "OCAR";
        break;
      case 5:
        return "OCRA";
        break;
      case 6:
        return "CAOR";
        break;
      case 7:
        return "CARO";
        break;
      case 8:
        return "COAR";
        break;
      case 9:
        return "CORA";
        break;
      case 10:
        return "CRAO";
        break;
      case 11:
        return "CROA";
        break;
      default:
        return "Unknown";
    }
  }
};

/**
 * Module: audio thread.
 */
struct AocrModule : rack::engine::Module {
  enum ParamIds {      // presets use param index
    ORDER_PARAM,       // param 0
    ATV_PARAM,         // param 1
    ATV_CV_ATV_PARAM,  // param 2
    OFS_PARAM,         // param 3
    OFS_CV_ATV_PARAM,  // param 4
    CLIP_PARAM,        // param 5
    RECT_PARAM,        // param 6
    RTYPE_PARAM,       // param 7
    NUM_PARAMS
  };
  enum InputIds { SGNL_INPUT, ATV_CV_INPUT, OFS_CV_INPUT, NUM_INPUTS };
  enum OutputIds { SGNL_OUTPUT, NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  int inputSignalNumChannels{0};
  rack::simd::float_4 inputSignalGridLights[DANT::SIMD];
  rack::simd::float_4 outputSignalGridLights[DANT::SIMD];

  /**
   * Module constructor.
   */
  AocrModule() {
    rack::engine::Module::config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    rack::engine::Module::configParam<OpOrderQuantity>(ORDER_PARAM, DANT::OP_ORDER::AOCR, DANT::OP_ORDER::CROA,
                                                       DANT::OP_ORDER::AOCR, "Operation Order");
    paramQuantities[ORDER_PARAM]->snapEnabled = true;

    rack::engine::Module::configParam(ATV_PARAM, -2.0f, 2.0f, 1.0f, "Signal Attenuverter");
    rack::engine::Module::configParam(ATV_CV_ATV_PARAM, -2.0f, 2.0f, 1.0f, "Attenuverter CV attenuverter", "x");
    rack::engine::Module::configParam(OFS_PARAM, -10.0f, 10.0f, 0.0f, "Signal Offset");
    rack::engine::Module::configParam(OFS_CV_ATV_PARAM, -2.0f, 2.0f, 1.0f, "Offset CV attenuverter", "x");

    rack::engine::Module::configSwitch(CLIP_PARAM, DANT::CLIP_LVL::NO_CLIP, DANT::CLIP_LVL::FIVE_CLIP,
                                       DANT::CLIP_LVL::NO_CLIP, "Clipping", {"None", "±10 volts", "±5 volts"});
    rack::engine::Module::configSwitch(RECT_PARAM, DANT::RECT_LVL::NO_RECT, DANT::RECT_LVL::FULL_RECT,
                                       DANT::RECT_LVL::NO_RECT, "Rectify", {"None", "Half", "Full"});
    rack::engine::Module::configSwitch(RTYPE_PARAM, DANT::RECT_TYPE::POS_RECT, DANT::RECT_TYPE::NEG_RECT,
                                       DANT::RECT_TYPE::POS_RECT, "Rectify direction", {"Positive", "Negative"});

    rack::engine::Module::configInput(SGNL_INPUT, "[Poly] Signal");
    rack::engine::Module::configInput(ATV_CV_INPUT, "Attenuveter CV");
    rack::engine::Module::configInput(OFS_CV_INPUT, "Offset CV");

    rack::engine::Module::configOutput(SGNL_OUTPUT, "[Poly] Signal");

    rack::engine::Module::configBypass(SGNL_INPUT, SGNL_OUTPUT);
  }

  /**
   * Module destructor.
   */
  ~AocrModule() {}

  /**
   * Called on autosave, store non-parameter module data.
   */
  json_t *dataToJson() override {
    DANT::saveUserSettings();

    json_t *rootJ = json_object();

    return rootJ;
  }

  /**
   * Called when module is loaded, sets non-parameter module data.
   */
  void dataFromJson(json_t *rootJ) override { DANT::loadUserSettings(); }

  /**
   * Called when a preset is loaded.
   */
  void paramsFromJson(json_t *rootJ) override { rack::engine::Module::paramsFromJson(rootJ); }

  /**
   * Called when the application sample rate is changed.
   */
  void onSampleRateChange() override {}

  /**
   * Called on module randomise.
   */
  void onRandomize(const RandomizeEvent &e) override {
    rack::engine::Module::onRandomize(e);  // randomise all parameters, manually implement non-paramtere randomisation.
  }

  /**
   * Called for module initialisation, can be called to manually reset params.
   */
  void onReset() override {
    softReset();

    rack::engine::Module::onReset();
  }

  /**
   * Can be called to reset non-parameter data.
   */
  void softReset() {
    inputSignalNumChannels = 0;
    resetArrays();
  }

  void resetArrays() {
    std::fill(inputSignalGridLights, inputSignalGridLights + DANT::SIMD, DANT::SIMD_ZERO);
    std::fill(outputSignalGridLights, outputSignalGridLights + DANT::SIMD, DANT::SIMD_ZERO);
  }

  /**
   * Called every sample, run DSP code.
   */
  void process(const rack::engine::Module::ProcessArgs &args) override {
    inputSignalNumChannels = inputs[SGNL_INPUT].getChannels();

    DANT::AOCROpts processOptions;
    processOptions.opOrder = readOrdering();
    processOptions.attenuversion = readAttenuverter();
    processOptions.offset = readOffset();
    processOptions.clipLvl = readClipping();
    processOptions.rectLvl = readRectification();
    processOptions.rectType = readRectifyType();

    for (int c{0}; c < inputSignalNumChannels; c += DANT::SIMD) {
      rack::simd::float_4 inputSignals = inputs[SGNL_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
      inputSignalGridLights[DANT::SIMD_I[c]] = inputSignals;

      rack::simd::float_4 processedVals = DANT::attenuvertOffsetClipRectify(inputSignals, processOptions);

      outputSignalGridLights[DANT::SIMD_I[c]] = processedVals;
      outputs[SGNL_OUTPUT].setVoltageSimd<rack::simd::float_4>(processedVals, c);
    }

    outputs[SGNL_OUTPUT].setChannels(inputSignalNumChannels);
  }

  // converts between parameter int value and dsp code enum
  inline DANT::OP_ORDER readOrdering() {
    switch (static_cast<int>(params[ORDER_PARAM].getValue())) {
      case 0:
        return DANT::OP_ORDER::AOCR;
        break;
      case 1:
        return DANT::OP_ORDER::ACOR;
        break;
      case 2:
        return DANT::OP_ORDER::ACRO;
        break;
      case 3:
        return DANT::OP_ORDER::OACR;
        break;
      case 4:
        return DANT::OP_ORDER::OCAR;
        break;
      case 5:
        return DANT::OP_ORDER::OCRA;
        break;
      case 6:
        return DANT::OP_ORDER::CAOR;
        break;
      case 7:
        return DANT::OP_ORDER::CARO;
        break;
      case 8:
        return DANT::OP_ORDER::COAR;
        break;
      case 9:
        return DANT::OP_ORDER::CORA;
        break;
      case 10:
        return DANT::OP_ORDER::CRAO;
        break;
      case 11:
        return DANT::OP_ORDER::CROA;
        break;
      default:
        return DANT::OP_ORDER::AOCR;
        break;
    }
  }

  // calculates the attenuversion value from the parameter plus the attenuverted CV
  inline float readAttenuverter() {
    return params[ATV_PARAM].getValue() +
           (inputs[ATV_CV_INPUT].getNormalVoltage(0.0f) * params[ATV_CV_ATV_PARAM].getValue());
  }

  // calculates the offset value from the parameter plus the offset CV
  inline float readOffset() {
    return params[OFS_PARAM].getValue() +
           (inputs[OFS_CV_INPUT].getNormalVoltage(0.0f) * params[OFS_CV_ATV_PARAM].getValue());
  }

  // converts between parameter int value and dsp code enum
  inline DANT::CLIP_LVL readClipping() {
    switch (static_cast<int>(params[CLIP_PARAM].getValue())) {
      case 0:
        return DANT::CLIP_LVL::NO_CLIP;
        break;
      case 1:
        return DANT::CLIP_LVL::TEN_CLIP;
        break;
      case 2:
        return DANT::CLIP_LVL::FIVE_CLIP;
        break;
      default:
        return DANT::CLIP_LVL::NO_CLIP;
        break;
    }
  }

  // converts between parameter int value and dsp code enum
  inline DANT::RECT_LVL readRectification() {
    switch (static_cast<int>(params[RECT_PARAM].getValue())) {
      case 0:
        return DANT::RECT_LVL::NO_RECT;
        break;
      case 1:
        return DANT::RECT_LVL::HALF_RECT;
        break;
      case 2:
        return DANT::RECT_LVL::FULL_RECT;
        break;
      default:
        return DANT::RECT_LVL::NO_RECT;
        break;
    }
  }

  // converts between parameter int value and dsp code enum
  inline DANT::RECT_TYPE readRectifyType() {
    switch (static_cast<int>(params[RTYPE_PARAM].getValue())) {
      case 0:
        return DANT::RECT_TYPE::POS_RECT;
        break;
      case 1:
        return DANT::RECT_TYPE::NEG_RECT;
        break;
      default:
        return DANT::RECT_TYPE::POS_RECT;
        break;
    }
  }
};

/**
 * Widgets: UI thread.
 */
struct OpOrderWidget : rack::widget::TransparentWidget {
  AocrModule *module;

  OpOrderWidget(AocrModule *m) { this->module = m; }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    nvgSave(args.vg);

    nvgStrokeColor(args.vg, nvgRGB(0, 0, 0));
    nvgStrokeWidth(args.vg, 1.0f);

    DANT::Fonts::DrawOptions opts;
    opts.align = NVG_ALIGN_TOP | NVG_ALIGN_CENTER;
    opts.ttfFile = DANT::REGULAR_TTF;
    opts.size = 9.0f;

    opts.xpos = this->box.size.x * 0.5f;
    opts.ypos = 0.0f;
    DANT::Fonts::drawText(args, "order", opts);

    opts.align = NVG_ALIGN_TOP | NVG_ALIGN_LEFT;
    opts.xpos = 0.0f;

    opts.ypos = this->box.size.y * 0.2f;
    DANT::Fonts::drawText(args, "1", opts);

    opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
    DANT::Fonts::drawText(args, "2", opts);

    opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
    DANT::Fonts::drawText(args, "3", opts);

    opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
    DANT::Fonts::drawText(args, "4", opts);

    nvgRestore(args.vg);
  }

  void drawLayer(const rack::widget::Widget::DrawArgs &args, int layer) override {
    if (layer == 1) {
      nvgSave(args.vg);

      DANT::Fonts::DrawOptions opts;
      opts.align = NVG_ALIGN_TOP | NVG_ALIGN_RIGHT;
      opts.ttfFile = DANT::REGULAR_TTF;
      opts.size = 9.0f;
      opts.colour = DANT::RGB_CV_YELLOW;

      DANT::OP_ORDER currentOrder = DANT::OP_ORDER::AOCR;
      if (module) {
        currentOrder = module->readOrdering();
      }

      switch (currentOrder) {
        case DANT::OP_ORDER::AOCR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::ACOR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::ACRO:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::OACR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::OCAR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::OCRA:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::CAOR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::CARO:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::COAR:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::CORA:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::CRAO:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        case DANT::OP_ORDER::CROA:
          opts.xpos = (this->box.size.x * 0.25f) * 1.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 4.0f;
          DANT::Fonts::drawText(args, "A", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 2.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 3.0f;
          DANT::Fonts::drawText(args, "O", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 3.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 1.0f;
          DANT::Fonts::drawText(args, "C", opts);
          opts.xpos = (this->box.size.x * 0.25f) * 4.0f;
          opts.ypos = (this->box.size.y * 0.2f) * 2.0f;
          DANT::Fonts::drawText(args, "R", opts);
          break;
        default:
          break;
      }

      nvgRestore(args.vg);
    }
    rack::widget::Widget::drawLayer(args, layer);
  }
};

struct AocrWidget : DANT::ModuleWidget {
  /**
   * Component widgets.
   */
  DANT::Port *testInputPort;
  DANT::GridLight *signalInGridLight;
  DANT::Knob *orderKnob;
  DANT::Knob *attenuverterKnob;
  DANT::Port *attenuverterCvInput;
  DANT::Trimpot *attenuverterCvTrimpot;
  DANT::Knob *offsetKnob;
  DANT::Port *offsetCvInput;
  DANT::Trimpot *offsetCvTrimpot;
  rack::componentlibrary::CKSSThree *clipSwitch;
  rack::componentlibrary::CKSS *rectifyTypeSwitch;
  rack::componentlibrary::CKSSThree *rectifySwitch;
  DANT::GridLight *signalOutGridLight;
  DANT::Port *testOutputPort;

  /**
   * Widget constructor.
   */
  AocrWidget(AocrModule *module) {
    rack::app::ModuleWidget::setModule(module);

    this->box.size = rack::math::Vec(rack::app::RACK_GRID_WIDTH * HP, rack::app::RACK_GRID_HEIGHT);

    // sub-widgets
    {
      OpOrderWidget *orderDisplay = new OpOrderWidget(module);
      orderDisplay->setPosition(DANT::layout(1.0f, 2.75f));
      orderDisplay->setSize(DANT::layout(4.5f, 2.0f));
      addChild(orderDisplay);
    }

    // construct components
    testInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(3.0f, 2.0f), module, AocrModule::SGNL_INPUT);

    signalInGridLight = rack::createWidgetCentered<DANT::GridLight>(DANT::layout(4.5f, 2.0f));
    signalInGridLight->fullMode();
    if (module) {
      signalInGridLight->numChannels = &module->inputSignalNumChannels;
      signalInGridLight->channelValues = module->inputSignalGridLights;  // array decays into pointer to 1st element
    }

    orderKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(3.0f, 5.0f), module, AocrModule::ORDER_PARAM);
    orderKnob->vizType = DANT::KnobViz::NOTCHES;
    orderKnob->numNotches = 12;

    attenuverterKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(2.0f, 7.0f), module, AocrModule::ATV_PARAM);
    attenuverterKnob->vizType = DANT::KnobViz::BIPARC;
    attenuverterKnob->inputId = AocrModule::ATV_CV_INPUT;
    attenuverterKnob->cvAttId = AocrModule::ATV_CV_ATV_PARAM;

    attenuverterCvTrimpot =
        rack::createParamCentered<DANT::Trimpot>(DANT::layout(4.0f, 6.0f), module, AocrModule::ATV_CV_ATV_PARAM);

    attenuverterCvInput =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.0f, 7.0f), module, AocrModule::ATV_CV_INPUT);

    offsetKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(4.0f, 10.0f), module, AocrModule::OFS_PARAM);
    offsetKnob->vizType = DANT::KnobViz::BIPARC;
    offsetKnob->numNotches = 3;
    offsetKnob->inputId = AocrModule::OFS_CV_INPUT;
    offsetKnob->cvAttId = AocrModule::OFS_CV_ATV_PARAM;

    offsetCvTrimpot =
        rack::createParamCentered<DANT::Trimpot>(DANT::layout(2.0f, 9.0f), module, AocrModule::OFS_CV_ATV_PARAM);

    offsetCvInput = rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 10.0f), module, AocrModule::OFS_CV_INPUT);

    clipSwitch = rack::createParamCentered<rack::componentlibrary::CKSSThree>(DANT::layout(2.25f, 12.0f), module,
                                                                              AocrModule::CLIP_PARAM);

    rectifyTypeSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(2.25f, 13.5f), module,
                                                                                AocrModule::RTYPE_PARAM);

    rectifySwitch = rack::createParamCentered<rack::componentlibrary::CKSSThree>(DANT::layout(4.75f, 13.5f), module,
                                                                                 AocrModule::RECT_PARAM);

    signalOutGridLight = rack::createWidgetCentered<DANT::GridLight>(DANT::layout(1.5f, 15.0f));
    signalOutGridLight->fullMode();
    if (module) {
      signalOutGridLight->numChannels = &module->inputSignalNumChannels;
      signalOutGridLight->channelValues = module->outputSignalGridLights;  // array decays into pointer to 1st element
    }

    testOutputPort = rack::createOutputCentered<DANT::Port>(DANT::layout(3.0f, 15.0f), module, AocrModule::SGNL_OUTPUT);
    testOutputPort->isOutput = true;

    // add components
    rack::app::ModuleWidget::addInput(testInputPort);
    rack::app::ModuleWidget::addChild(signalInGridLight);
    rack::app::ModuleWidget::addParam(orderKnob);
    rack::app::ModuleWidget::addParam(attenuverterKnob);
    rack::app::ModuleWidget::addInput(attenuverterCvInput);
    rack::app::ModuleWidget::addParam(attenuverterCvTrimpot);
    rack::app::ModuleWidget::addParam(offsetKnob);
    rack::app::ModuleWidget::addInput(offsetCvInput);
    rack::app::ModuleWidget::addParam(offsetCvTrimpot);
    rack::app::ModuleWidget::addParam(clipSwitch);
    rack::app::ModuleWidget::addParam(rectifySwitch);
    rack::app::ModuleWidget::addParam(rectifyTypeSwitch);
    rack::app::ModuleWidget::addChild(signalOutGridLight);
    rack::app::ModuleWidget::addOutput(testOutputPort);
  }

  // used by the common code to draw the module title
  std::string moduleName() override { return "AOCR"; }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    DANT::ModuleWidget::draw(args);  // call common draw method for panel first

    // now draw on top of the panel
    DANT::Fonts::DrawOptions opts;
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;
    opts.size = 20.0f;

    opts.xpos = DANT::layout(1.5f, 2.0f).x;
    opts.ypos = DANT::layout(1.5f, 2.0f).y;
    // input icon
    DANT::Fonts::drawSymbols(args, {"input_circle"}, opts);

    opts.xpos = DANT::layout(4.5f, 15.0f).x;
    opts.ypos = DANT::layout(4.5f, 15.0f).y;
    // output icon
    DANT::Fonts::drawSymbols(args, {"output_circle"}, opts);

    // text settings
    opts.size = 10.0f;
    opts.ttfFile = DANT::REGULAR_TTF;

    // clip options
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT;
    opts.xpos = DANT::layout(2.8f, 11.6f).x;
    opts.ypos = DANT::layout(2.8f, 11.6f).y;
    DANT::Fonts::drawText(args, "±5v", opts);
    opts.xpos = DANT::layout(2.8f, 12.0f).x;
    opts.ypos = DANT::layout(2.8f, 12.0f).y;
    DANT::Fonts::drawText(args, "±10v", opts);
    opts.xpos = DANT::layout(2.8f, 12.4f).x;
    opts.ypos = DANT::layout(2.8f, 12.4f).y;
    DANT::Fonts::drawText(args, "none", opts);

    // rectify options
    opts.size = 9.5f;
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT;
    opts.xpos = DANT::layout(1.75f, 13.3f).x;
    opts.ypos = DANT::layout(1.75f, 13.3f).y;
    DANT::Fonts::drawText(args, "[-]", opts);
    opts.xpos = DANT::layout(1.75f, 13.7f).x;
    opts.ypos = DANT::layout(1.75f, 13.7f).y;
    DANT::Fonts::drawText(args, "[+]", opts);
    opts.xpos = DANT::layout(4.15f, 13.1f).x;
    opts.ypos = DANT::layout(4.15f, 13.1f).y;
    DANT::Fonts::drawText(args, "Full", opts);
    opts.xpos = DANT::layout(4.15f, 13.5f).x;
    opts.ypos = DANT::layout(4.15f, 13.5f).y;
    DANT::Fonts::drawText(args, "Half", opts);
    opts.xpos = DANT::layout(4.15f, 13.9f).x;
    opts.ypos = DANT::layout(4.15f, 13.9f).y;
    DANT::Fonts::drawText(args, "none", opts);
  }
};

/**
 * Create model and register with the plugin.
 */
rack::plugin::Model *modelAocr = rack::createModel<AocrModule, AocrWidget>("AOCR");
