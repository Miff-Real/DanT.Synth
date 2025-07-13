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
 * Module: audio thread.
 */
struct AocrModule : rack::engine::Module {
  enum ParamIds {      // presets use param index
    ATV_PARAM,         // param 0
    ATV_CV_ATV_PARAM,  // param 1
    OFS_PARAM,         // param 2
    OFS_CV_ATV_PARAM,  // param 3
    ORDER_PARAM,       // param 4
    CLIP_PARAM,        // param 5
    RECT_PARAM,        // param 6
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

    rack::engine::Module::configParam(ATV_PARAM, -2.0f, 2.0f, 1.0f, "Signal Attenuverter");
    rack::engine::Module::configParam(ATV_CV_ATV_PARAM, -2.0f, 2.0f, 1.0f, "Attenuverter CV attenuverter", "x");
    rack::engine::Module::configParam(OFS_PARAM, -10.0f, 10.0f, 0.0f, "Signal Offset");
    rack::engine::Module::configParam(OFS_CV_ATV_PARAM, -2.0f, 2.0f, 1.0f, "Offset CV attenuverter", "x");

    rack::engine::Module::configSwitch(ORDER_PARAM, DANT::OP_ORDER::AT_THEN_OFF, DANT::OP_ORDER::OFF_THEN_AT,
                                       DANT::OP_ORDER::AT_THEN_OFF, "Attenuvert-Offset order",
                                       {"Post offset", "Pre offset"});
    rack::engine::Module::configSwitch(CLIP_PARAM, DANT::CLIP_LVL::NO_CLIP, DANT::CLIP_LVL::FIVE_CLIP,
                                       DANT::CLIP_LVL::NO_CLIP, "Clipping", {"None", "±10 volts", "±5 volts"});
    rack::engine::Module::configSwitch(RECT_PARAM, DANT::RECT_LVL::NO_RECT, DANT::RECT_LVL::FULL_RECT,
                                       DANT::RECT_LVL::NO_RECT, "Rectify", {"None", "Half", "Full"});

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

    DANT::AtOCROpts processOptions;
    processOptions.attenuversion = readAttenuverter();
    processOptions.offset = readOffset();
    processOptions.opOrder = readOrdering();
    processOptions.clipLvl = readClipping();
    processOptions.rectLvl = readRectification();

    for (int c{0}; c < inputSignalNumChannels; c += DANT::SIMD) {
      rack::simd::float_4 inputSignals = inputs[SGNL_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
      inputSignalGridLights[DANT::SIMD_I[c]] = inputSignals;

      rack::simd::float_4 processedVals = DANT::attenuvertOffsetClipRectify(inputSignals, processOptions);

      outputSignalGridLights[DANT::SIMD_I[c]] = processedVals;
      outputs[SGNL_OUTPUT].setVoltageSimd<rack::simd::float_4>(processedVals, c);
    }

    outputs[SGNL_OUTPUT].setChannels(inputSignalNumChannels);
  }

  // calculates the attenuversion value from the parameter plus the attenuverted CV
  float readAttenuverter() {
    return params[ATV_PARAM].getValue() +
           (inputs[ATV_CV_INPUT].getNormalVoltage(0.0f) * params[ATV_CV_ATV_PARAM].getValue());
  }

  // calculates the offset value from the parameter plus the offset CV
  float readOffset() {
    return params[OFS_PARAM].getValue() +
           (inputs[OFS_CV_INPUT].getNormalVoltage(0.0f) * params[OFS_CV_ATV_PARAM].getValue());
  }

  // converts between parameter int value and dsp code enum
  DANT::OP_ORDER readOrdering() {
    switch (static_cast<int>(params[ORDER_PARAM].getValue())) {
      case 0:
        return DANT::OP_ORDER::AT_THEN_OFF;
        break;
      case 1:
        return DANT::OP_ORDER::OFF_THEN_AT;
        break;
      default:
        return DANT::OP_ORDER::AT_THEN_OFF;
        break;
    }
  }

  // converts between parameter int value and dsp code enum
  DANT::CLIP_LVL readClipping() {
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
  DANT::RECT_LVL readRectification() {
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
};

/**
 * Widget: UI thread.
 */
struct AocrWidget : DANT::ModuleWidget {
  /**
   * Component widgets.
   */
  DANT::Port *testInputPort;
  DANT::GridLight *signalInGridLight;
  DANT::Knob *attenuverterKnob;
  DANT::Port *attenuverterCvInput;
  DANT::Trimpot *attenuverterCvTrimpot;
  rack::componentlibrary::CKSS *orderSwitch;
  DANT::Knob *offsetKnob;
  DANT::Port *offsetCvInput;
  DANT::Trimpot *offsetCvTrimpot;
  rack::componentlibrary::CKSSThree *clipSwitch;
  rack::componentlibrary::CKSSThree *rectifySwitch;
  DANT::GridLight *signalOutGridLight;
  DANT::Port *testOutputPort;

  /**
   * Widget constructor.
   */
  AocrWidget(AocrModule *module) {
    rack::app::ModuleWidget::setModule(module);

    this->box.size = rack::math::Vec(rack::app::RACK_GRID_WIDTH * HP, rack::app::RACK_GRID_HEIGHT);

    // construct components
    testInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(3.0f, 2.0f), module, AocrModule::SGNL_INPUT);

    signalInGridLight = rack::createWidgetCentered<DANT::GridLight>(DANT::layout(4.5f, 2.0f));
    signalInGridLight->fullMode();
    if (module) {
      signalInGridLight->numChannels = &module->inputSignalNumChannels;
      signalInGridLight->channelValues = module->inputSignalGridLights;  // array decays into pointer to 1st element
    }

    attenuverterKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(2.0f, 5.0f), module, AocrModule::ATV_PARAM);
    attenuverterKnob->vizType = DANT::BIPARC;
    attenuverterKnob->inputId = AocrModule::ATV_CV_INPUT;
    attenuverterKnob->cvAttId = AocrModule::ATV_CV_ATV_PARAM;

    attenuverterCvTrimpot =
        rack::createParamCentered<DANT::Trimpot>(DANT::layout(4.0f, 4.0f), module, AocrModule::ATV_CV_ATV_PARAM);

    attenuverterCvInput =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.0f, 5.0f), module, AocrModule::ATV_CV_INPUT);

    orderSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(4.5f, 6.5f), module,
                                                                          AocrModule::ORDER_PARAM);

    offsetKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(4.0f, 8.5f), module, AocrModule::OFS_PARAM);
    offsetKnob->vizType = DANT::BIPARC;
    offsetKnob->numNotches = 3;
    offsetKnob->inputId = AocrModule::OFS_CV_INPUT;
    offsetKnob->cvAttId = AocrModule::OFS_CV_ATV_PARAM;

    offsetCvTrimpot =
        rack::createParamCentered<DANT::Trimpot>(DANT::layout(2.0f, 7.5f), module, AocrModule::OFS_CV_ATV_PARAM);

    offsetCvInput = rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 8.5f), module, AocrModule::OFS_CV_INPUT);

    clipSwitch = rack::createParamCentered<rack::componentlibrary::CKSSThree>(DANT::layout(1.5f, 10.0f), module,
                                                                              AocrModule::CLIP_PARAM);

    rectifySwitch = rack::createParamCentered<rack::componentlibrary::CKSSThree>(DANT::layout(4.5f, 12.0f), module,
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
    rack::app::ModuleWidget::addParam(attenuverterKnob);
    rack::app::ModuleWidget::addInput(attenuverterCvInput);
    rack::app::ModuleWidget::addParam(attenuverterCvTrimpot);
    rack::app::ModuleWidget::addParam(orderSwitch);
    rack::app::ModuleWidget::addParam(offsetKnob);
    rack::app::ModuleWidget::addInput(offsetCvInput);
    rack::app::ModuleWidget::addParam(offsetCvTrimpot);
    rack::app::ModuleWidget::addParam(clipSwitch);
    rack::app::ModuleWidget::addParam(rectifySwitch);
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

    // order options
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT;
    opts.xpos = DANT::layout(3.9f, 6.3f).x;
    opts.ypos = DANT::layout(3.9f, 6.3f).y;
    DANT::Fonts::drawText(args, "Pre", opts);
    opts.xpos = DANT::layout(3.9f, 6.7f).x;
    opts.ypos = DANT::layout(3.9f, 6.7f).y;
    DANT::Fonts::drawText(args, "Post", opts);

    // clip options
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT;
    opts.xpos = DANT::layout(2.1f, 9.6f).x;
    opts.ypos = DANT::layout(2.1f, 9.6f).y;
    DANT::Fonts::drawText(args, "±5v", opts);
    opts.xpos = DANT::layout(2.1f, 10.0f).x;
    opts.ypos = DANT::layout(2.1f, 10.0f).y;
    DANT::Fonts::drawText(args, "±10v", opts);
    opts.xpos = DANT::layout(2.1f, 10.4f).x;
    opts.ypos = DANT::layout(2.1f, 10.4f).y;
    DANT::Fonts::drawText(args, "none", opts);

    // clip options
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT;
    opts.xpos = DANT::layout(3.9f, 11.6f).x;
    opts.ypos = DANT::layout(3.9f, 11.6f).y;
    DANT::Fonts::drawText(args, "Full", opts);
    opts.xpos = DANT::layout(3.9f, 12.0f).x;
    opts.ypos = DANT::layout(3.9f, 12.0f).y;
    DANT::Fonts::drawText(args, "Half", opts);
    opts.xpos = DANT::layout(3.9f, 12.4f).x;
    opts.ypos = DANT::layout(3.9f, 12.4f).y;
    DANT::Fonts::drawText(args, "none", opts);
  }
};

/**
 * Create model and register with the plugin.
 */
rack::plugin::Model *modelAtocr = rack::createModel<AocrModule, AocrWidget>("AOCR");
