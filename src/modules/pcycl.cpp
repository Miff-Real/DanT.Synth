#include <algorithm>  // std::fill std::max
#include <cmath>      // std::exp
#include <string>

#include "../dsp/poly-cycle.hpp"
#include "../plugin.hpp"
#include "../shared/grid-light.hpp"
#include "../shared/knob.hpp"
#include "../shared/module-widget.hpp"
#include "../shared/port.hpp"

/**
 * Constant values.
 */
const int HP{3};
// VCV timing standard, events for the same note can be up to 1ms apart
const float PCYCL_WINDOW_SECONDS{0.001f};
const float PCYCL_ACTIVE_LIGHT{10.0f};
const float PCYCL_MIN_THRESHOLD{0.01f};
const float PCYCL_MAX_THRESHOLD{2.0f};
const float PCYCL_DEFAULT_THRESHOLD{1.0f / 12.0f};  // one semitone
const float PCYCL_MIN_GATE_SECONDS{0.01f};
const float PCYCL_MAX_GATE_SECONDS{2.0f};
const float PCYCL_DEFAULT_GATE_SECONDS{0.1f};

/**
 * Quantity for the change threshold menu slider.
 */
struct PcyclThresholdQuantity : DANT::FloatValueQuantity {
  using DANT::FloatValueQuantity::FloatValueQuantity;

  // most signals patched here are V/Oct, so the equivalent pitch is shown beside the voltage
  std::string getDisplayValueString() override {
    return rack::string::f("%.3fV (%.1f semitones)", getValue(), getValue() * 12.0f);
  }
};

/**
 * Module: audio thread.
 */
struct PcyclModule : rack::engine::Module {
  enum ParamIds {  // presets use param index
    CHANS_PARAM,   // param 0 - number of output channels
    RESET_PARAM,   // param 1 - manual reset button
    NUM_PARAMS
  };
  enum InputIds {
    SGNL_INPUT,   // mono - signal to route
    TRIG_INPUT,   // mono - moves the signal to the next channel
    RESET_INPUT,  // mono - returns the signal to the first channel
    NUM_INPUTS
  };
  enum OutputIds {
    SGNL_OUTPUT,  // poly - routed signal
    TRIG_OUTPUT,  // poly - trigger input, on the same channel as the signal
    NUM_OUTPUTS
  };
  enum LightIds { RESET_LIGHT, NUM_LIGHTS };

  DANT::PolyCycle cycle;
  DANT::IDLE_MODE idleMode{DANT::IDLE_HOLD};
  bool autoAlign{true};
  float manualDelay{0.0f};  // samples, a float so that the menu slider can write to it
  bool advanceOnChange{false};
  float changeThreshold{PCYCL_DEFAULT_THRESHOLD};
  // what the trigger output sends, saved by index so only append to it
  enum TrigOutMode { TRIG_OUT_PASS, TRIG_OUT_OFF, TRIG_OUT_TRIGGER, TRIG_OUT_GATE, NUM_TRIG_OUT_MODES };
  TrigOutMode trigOutMode{TRIG_OUT_PASS};
  float gateSeconds{PCYCL_DEFAULT_GATE_SECONDS};

  rack::dsp::SchmittTrigger trigDetector;
  rack::dsp::SchmittTrigger resetDetector;
  rack::dsp::BooleanTrigger resetButtonDetector;

  float knownSampleRate{0.0f};
  int windowSamples{48};
  float followCoeff{0.02f};

  int gridLightChannels{1};
  rack::simd::float_4 gridLightValues[DANT::SIMD];

  /**
   * Module constructor.
   */
  PcyclModule() {
    rack::engine::Module::config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    rack::engine::Module::configParam(CHANS_PARAM, 1.0f, static_cast<float>(DANT::CHANS), 4.0f, "Channels");
    paramQuantities[CHANS_PARAM]->snapEnabled = true;
    // the channel count shapes the rest of the patch, randomising it is never what the user wants
    paramQuantities[CHANS_PARAM]->randomizeEnabled = false;

    rack::engine::Module::configButton(RESET_PARAM, "Reset");

    rack::engine::Module::configInput(SGNL_INPUT, "Signal");
    rack::engine::Module::configInput(TRIG_INPUT, "Next channel trigger");
    rack::engine::Module::configInput(RESET_INPUT, "Reset trigger");

    rack::engine::Module::configOutput(SGNL_OUTPUT, "[Poly] Signal");
    rack::engine::Module::configOutput(TRIG_OUTPUT, "[Poly] Trigger");

    rack::engine::Module::configBypass(SGNL_INPUT, SGNL_OUTPUT);
    rack::engine::Module::configBypass(TRIG_INPUT, TRIG_OUTPUT);

    resetArrays();
  }

  /**
   * Called on autosave, store non-parameter module data.
   */
  json_t* dataToJson() override {
    DANT::saveUserSettings();

    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "idleMode", json_integer(static_cast<int>(idleMode)));
    json_object_set_new(rootJ, "autoAlign", json_boolean(autoAlign));
    json_object_set_new(rootJ, "signalDelay", json_integer(readManualDelay()));
    json_object_set_new(rootJ, "advanceOnChange", json_boolean(advanceOnChange));
    json_object_set_new(rootJ, "changeThreshold", json_real(static_cast<double>(changeThreshold)));
    json_object_set_new(rootJ, "trigOutMode", json_integer(static_cast<int>(trigOutMode)));
    json_object_set_new(rootJ, "gateSeconds", json_real(static_cast<double>(gateSeconds)));

    return rootJ;
  }

  /**
   * Called when module is loaded, sets non-parameter module data.
   */
  void dataFromJson(json_t* rootJ) override {
    DANT::loadUserSettings();

    if (json_t* j = json_object_get(rootJ, "idleMode")) {
      idleMode = json_integer_value(j) == DANT::IDLE_ZERO ? DANT::IDLE_ZERO : DANT::IDLE_HOLD;
    }
    if (json_t* j = json_object_get(rootJ, "autoAlign")) {
      autoAlign = json_boolean_value(j);
    }
    if (json_t* j = json_object_get(rootJ, "signalDelay")) {
      manualDelay = rack::math::clamp(static_cast<float>(json_integer_value(j)), 0.0f,
                                      static_cast<float>(DANT::POLY_CYCLE_MAX_DELAY));
    }
    if (json_t* j = json_object_get(rootJ, "advanceOnChange")) {
      advanceOnChange = json_boolean_value(j);
    }
    if (json_t* j = json_object_get(rootJ, "changeThreshold")) {
      changeThreshold =
          rack::math::clamp(static_cast<float>(json_number_value(j)), PCYCL_MIN_THRESHOLD, PCYCL_MAX_THRESHOLD);
    }
    if (json_t* j = json_object_get(rootJ, "trigOutMode")) {
      const int mode{static_cast<int>(json_integer_value(j))};
      trigOutMode = (mode >= 0 && mode < NUM_TRIG_OUT_MODES) ? static_cast<TrigOutMode>(mode) : TRIG_OUT_PASS;
    }
    if (json_t* j = json_object_get(rootJ, "gateSeconds")) {
      gateSeconds =
          rack::math::clamp(static_cast<float>(json_number_value(j)), PCYCL_MIN_GATE_SECONDS, PCYCL_MAX_GATE_SECONDS);
    }
  }

  /**
   * Called for module initialisation, can be called to manually reset params.
   */
  void onReset() override {
    softReset();
    idleMode = DANT::IDLE_HOLD;
    autoAlign = true;
    manualDelay = 0.0f;
    advanceOnChange = false;
    changeThreshold = PCYCL_DEFAULT_THRESHOLD;
    trigOutMode = TRIG_OUT_PASS;
    gateSeconds = PCYCL_DEFAULT_GATE_SECONDS;

    rack::engine::Module::onReset();
  }

  /**
   * Can be called to reset non-parameter data.
   */
  void softReset() {
    cycle.reset();
    trigDetector.reset();
    resetDetector.reset();
    resetButtonDetector.reset();
    resetArrays();
  }

  void resetArrays() { std::fill(gridLightValues, gridLightValues + DANT::SIMD, DANT::SIMD_ZERO); }

  /**
   * Called every sample, run DSP code.
   */
  void process(const rack::engine::Module::ProcessArgs& args) override {
    if (args.sampleRate != knownSampleRate) {
      updateSampleRate(args.sampleRate);
    }

    DANT::PolyCycleOpts processOptions;
    processOptions.channels = readChannels();
    processOptions.idleMode = idleMode;
    processOptions.autoAlign = autoAlign;
    processOptions.delaySamples = readManualDelay();
    processOptions.advanceOnChange = advanceOnChange;
    processOptions.changeThreshold = changeThreshold;
    processOptions.followCoeff = followCoeff;
    processOptions.windowSamples = windowSamples;
    setGateOptions(processOptions, args.sampleRate);

    const float trigVoltage = inputs[TRIG_INPUT].getVoltage();
    const bool triggered = trigDetector.process(trigVoltage, 0.1f, 2.0f);

    cycle.step(inputs[SGNL_INPUT].getVoltage(), trigVoltage, triggered, readReset(), processOptions);

    outputs[SGNL_OUTPUT].setChannels(processOptions.channels);
    outputs[SGNL_OUTPUT].writeVoltages(cycle.outs);
    outputs[TRIG_OUTPUT].setChannels(processOptions.channels);
    outputs[TRIG_OUTPUT].writeVoltages(cycle.gateOuts);

    gridLightChannels = processOptions.channels;
    resetArrays();
    gridLightValues[DANT::SIMD_I[cycle.active]][DANT::SIMD_J[cycle.active]] = PCYCL_ACTIVE_LIGHT;

    const bool resetHeld = params[RESET_PARAM].getValue() > 0.0f || resetDetector.isHigh();
    lights[RESET_LIGHT].setSmoothBrightness(resetHeld ? 1.0f : 0.0f, args.sampleTime);
  }

  // the time based settings are kept in samples, so they only need working out when the sample rate changes
  inline void updateSampleRate(const float sampleRate) {
    knownSampleRate = sampleRate;
    windowSamples = std::max(1, static_cast<int>(sampleRate * PCYCL_WINDOW_SECONDS));
    followCoeff = 1.0f - std::exp(-1.0f / (sampleRate * PCYCL_WINDOW_SECONDS));
  }

  // a trigger and a gate are the same pulse to the DSP code, they only differ in length
  inline void setGateOptions(DANT::PolyCycleOpts& processOptions, const float sampleRate) {
    if (trigOutMode == TRIG_OUT_PASS) {
      processOptions.gateMode = DANT::GATE_PASS;
    } else if (trigOutMode == TRIG_OUT_OFF) {
      processOptions.gateMode = DANT::GATE_OFF;
    } else {
      processOptions.gateMode = DANT::GATE_PULSE;
      processOptions.gateSamples =
          trigOutMode == TRIG_OUT_TRIGGER ? windowSamples : std::max(1, static_cast<int>(gateSeconds * sampleRate));
    }
  }

  inline int readManualDelay() {
    return rack::math::clamp(static_cast<int>(manualDelay + 0.5f), 0, DANT::POLY_CYCLE_MAX_DELAY);
  }

  // the knob is snapped, rounding protects against values set by presets or parameter mapping
  inline int readChannels() {
    return rack::math::clamp(static_cast<int>(params[CHANS_PARAM].getValue() + 0.5f), 1, DANT::CHANS);
  }

  // both sources are always processed so that neither detector misses its edge
  inline bool readReset() {
    const bool fromInput = resetDetector.process(inputs[RESET_INPUT].getVoltage(), 0.1f, 2.0f);
    const bool fromButton = resetButtonDetector.process(params[RESET_PARAM].getValue() > 0.0f);
    return fromInput || fromButton;
  }
};

/**
 * Widgets: UI thread.
 */
static const std::string PCYCL_NEXT_CHANNEL{"\ue044"};
static const std::string PCYCL_RESET{"\uf56c"};

// describes what the automatic timing correction last measured, for the context menu
static std::string pcyclTimingLabel(const int skew) {
  if (skew == 0) {
    return "Measured: arriving together";
  }
  const int samples{skew < 0 ? -skew : skew};
  return rack::string::f("Measured: %s %d sample%s early", skew < 0 ? "signal" : "trigger", samples,
                         samples == 1 ? "" : "s");
}

struct PcyclChannelCountWidget : rack::widget::TransparentWidget {
  PcyclModule* module;

  PcyclChannelCountWidget(PcyclModule* m) { this->module = m; }

  // the lit number is hard to read against a bright panel without something behind it
  void draw(const rack::widget::Widget::DrawArgs& args) override {
    nvgSave(args.vg);

    NVGcolor countBG{DANT::Colours::getTextColour()};
    countBG.a = 0.5f;

    nvgFillColor(args.vg, countBG);
    nvgBeginPath(args.vg);
    nvgRoundedRect(args.vg, 0.0f, 0.0f, this->box.size.x, this->box.size.y, 4.0f);
    nvgFill(args.vg);

    nvgRestore(args.vg);
  }

  void drawLayer(const rack::widget::Widget::DrawArgs& args, int layer) override {
    if (layer == 1) {
      DANT::Fonts::DrawOptions opts;
      opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;
      opts.ttfFile = DANT::REGULAR_TTF;
      opts.size = 11.0f;
      opts.colour = DANT::RGB_CV_YELLOW;
      opts.xpos = this->box.size.x * 0.5f;
      opts.ypos = this->box.size.y * 0.5f;

      // the module browser has no module, show the default
      const int channels{module ? module->gridLightChannels : 4};
      DANT::Fonts::drawText(args, rack::string::f("%d", channels), opts);
    }
    rack::widget::Widget::drawLayer(args, layer);
  }
};

struct PcyclWidget : DANT::ModuleWidget {
  /**
   * Component widgets.
   */
  DANT::Port* signalInputPort;
  DANT::Port* trigInputPort;
  rack::componentlibrary::VCVLightButton<rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::RedLight>>*
      resetButton;
  DANT::Port* resetInputPort;
  DANT::Knob* channelsKnob;
  DANT::GridLight* activeGridLight;
  DANT::Port* signalOutputPort;
  DANT::Port* trigOutputPort;

  /**
   * Widget constructor.
   */
  PcyclWidget(PcyclModule* module) {
    rack::app::ModuleWidget::setModule(module);

    this->box.size = rack::math::Vec(rack::app::RACK_GRID_WIDTH * HP, rack::app::RACK_GRID_HEIGHT);

    // sub-widgets
    {
      PcyclChannelCountWidget* countDisplay = new PcyclChannelCountWidget(module);
      countDisplay->setSize(rack::math::Vec(20.0f, 13.0f));
      countDisplay->setPosition(DANT::layout(2.0f, 9.0f).minus(countDisplay->getSize().mult(0.5f)));
      addChild(countDisplay);
    }

    // construct components
    signalInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 2.95f), module, PcyclModule::SGNL_INPUT);

    trigInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 4.9f), module, PcyclModule::TRIG_INPUT);

    resetButton = rack::createLightParamCentered<rack::componentlibrary::VCVLightButton<
        rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::RedLight>>>(
        DANT::layout(2.0f, 6.8f), module, PcyclModule::RESET_PARAM, PcyclModule::RESET_LIGHT);

    resetInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 7.75f), module, PcyclModule::RESET_INPUT);

    channelsKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(2.0f, 9.9f), module, PcyclModule::CHANS_PARAM);
    channelsKnob->vizType = DANT::KnobViz::NOTCHES;
    channelsKnob->numNotches = DANT::CHANS;

    activeGridLight = rack::createWidgetCentered<DANT::GridLight>(DANT::layout(2.0f, 11.0f));
    activeGridLight->uniMode();
    if (module) {
      activeGridLight->numChannels = &module->gridLightChannels;
      activeGridLight->channelValues = module->gridLightValues;  // array decays into pointer to 1st element
    }

    signalOutputPort =
        rack::createOutputCentered<DANT::Port>(DANT::layout(2.0f, 12.95f), module, PcyclModule::SGNL_OUTPUT);
    signalOutputPort->isOutput = true;

    trigOutputPort =
        rack::createOutputCentered<DANT::Port>(DANT::layout(2.0f, 15.0f), module, PcyclModule::TRIG_OUTPUT);
    trigOutputPort->isOutput = true;

    // add components
    rack::app::ModuleWidget::addInput(signalInputPort);
    rack::app::ModuleWidget::addInput(trigInputPort);
    rack::app::ModuleWidget::addParam(resetButton);
    rack::app::ModuleWidget::addInput(resetInputPort);
    rack::app::ModuleWidget::addParam(channelsKnob);
    rack::app::ModuleWidget::addChild(activeGridLight);
    rack::app::ModuleWidget::addOutput(signalOutputPort);
    rack::app::ModuleWidget::addOutput(trigOutputPort);
  }

  // the panel is too narrow for the name to follow the logo, it is drawn on its own line instead
  std::string moduleName() override { return ""; }

  void draw(const rack::widget::Widget::DrawArgs& args) override {
    DANT::ModuleWidget::draw(args);  // call common draw method for panel first

    // now draw on top of the panel
    DANT::Fonts::DrawOptions opts;
    opts.ttfFile = DANT::REGULAR_TTF;
    opts.align = NVG_ALIGN_LEFT | NVG_ALIGN_TOP;
    opts.xpos = 1.5f;
    opts.ypos = 12.5f;
    DANT::Fonts::drawText(args, "PCycl", opts);

    opts = DANT::Fonts::DrawOptions();
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;
    opts.size = 20.0f;
    opts.xpos = DANT::layout(2.0f, 2.0f).x;

    // each icon sits above the port it describes, the trigger icon is repeated above its output
    opts.ypos = DANT::layout(2.0f, 2.0f).y;
    DANT::Fonts::drawSymbols(args, DANT::INPUT_CIRCLE, opts);

    opts.ypos = DANT::layout(2.0f, 4.0f).y;
    DANT::Fonts::drawSymbols(args, PCYCL_NEXT_CHANNEL, opts);

    opts.ypos = DANT::layout(2.0f, 6.0f).y;
    DANT::Fonts::drawSymbols(args, PCYCL_RESET, opts);

    opts.ypos = DANT::layout(2.0f, 12.0f).y;
    DANT::Fonts::drawSymbols(args, DANT::OUTPUT_CIRCLE, opts);

    opts.ypos = DANT::layout(2.0f, 14.05f).y;
    DANT::Fonts::drawSymbols(args, PCYCL_NEXT_CHANNEL, opts);
  }

  void appendContextMenu(rack::ui::Menu* menu) override {
    DANT::ModuleWidget::appendContextMenu(menu);
    PcyclModule* module = dynamic_cast<PcyclModule*>(this->module);
    if (!module) return;
    menu->addChild(new rack::ui::MenuSeparator);
    menu->addChild(rack::createIndexSubmenuItem(
        "Idle channels", {"Hold last value", "Zero volts"}, [=]() { return static_cast<size_t>(module->idleMode); },
        [=](size_t mode) { module->idleMode = mode == 1 ? DANT::IDLE_ZERO : DANT::IDLE_HOLD; }));
    menu->addChild(rack::createSubmenuItem("Timing correction", "", [=](rack::ui::Menu* menu) {
      menu->addChild(rack::createBoolPtrMenuItem("Automatic", "", &module->autoAlign));
      if (module->autoAlign) {
        menu->addChild(rack::createMenuLabel(pcyclTimingLabel(module->cycle.skew)));
      } else {
        menu->addChild(new DANT::MenuSlider(
            new DANT::FloatValueQuantity("Signal delay", 0.0f, static_cast<float>(DANT::POLY_CYCLE_MAX_DELAY), 0.0f,
                                         &module->manualDelay, " samples", 1.0f, "%.0f"),
            DANT::RGB_SLIDER_WIDTH));
      }
    }));
    menu->addChild(rack::createIndexSubmenuItem(
        "Trigger output", {"Pass trigger input", "Off", "Trigger for each note", "Gate for each note"},
        [=]() { return static_cast<size_t>(module->trigOutMode); },
        [=](size_t mode) { module->trigOutMode = static_cast<PcyclModule::TrigOutMode>(mode); }));
    menu->addChild(new DANT::MenuSlider(
        new DANT::FloatValueQuantity("Gate length", PCYCL_MIN_GATE_SECONDS, PCYCL_MAX_GATE_SECONDS,
                                     PCYCL_DEFAULT_GATE_SECONDS, &module->gateSeconds, "s", 1.0f, "%.2f"),
        DANT::RGB_SLIDER_WIDTH));
    menu->addChild(rack::createSubmenuItem("Automatic channel increment", "", [=](rack::ui::Menu* menu) {
      menu->addChild(rack::createBoolPtrMenuItem("Enabled", "", &module->advanceOnChange));
      menu->addChild(
          new DANT::MenuSlider(new PcyclThresholdQuantity("Threshold", PCYCL_MIN_THRESHOLD, PCYCL_MAX_THRESHOLD,
                                                          PCYCL_DEFAULT_THRESHOLD, &module->changeThreshold),
                               DANT::RGB_SLIDER_WIDTH));
    }));
  }
};

/**
 * Create model and register with the plugin.
 */
rack::plugin::Model* modelPcycl = rack::createModel<PcyclModule, PcyclWidget>("PCycl");
