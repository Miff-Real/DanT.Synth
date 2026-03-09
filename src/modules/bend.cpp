#include <jansson.h>

#include <string>
#include <vector>

#include "../dsp/bend-voct.hpp"
#include "../plugin.hpp"
#include "../shared/grid-light.hpp"
#include "../shared/knob.hpp"
#include "../shared/module-widget.hpp"
#include "../shared/port.hpp"

const int HP{8};

struct BeatDivision {
  float multiplier;
  std::string label;
};

const std::vector<BeatDivision> divisions = {
    {0.125f, "1/32"}, {0.166f, "1/16T"}, {0.25f, "1/16"}, {0.333f, "1/8T"}, {0.375f, "1/16."},
    {0.5f, "1/8"},    {0.75f, "1/8."},   {1.0f, "1/4"},   {2.0f, "1/2"},    {3.0f, "1/2."},
    {4.0f, "1/1"},    {5.0f, "5/4"},     {6.0f, "1/1."},  {7.0f, "7/4"},    {8.0f, "2/1"},
};

struct BeatDivQuantity : rack::ParamQuantity {
  std::string getDisplayValueString() override {
    int val = static_cast<int>(getValue() + (getValue() > 0.0f ? 0.5f : -0.5f));
    if (val >= 0) {
      int beats = val + 1;
      return divisions[rack::math::clamp(val + 7, 0, 14)].label + " - " + std::to_string(beats) +
             (beats == 1 ? " Beat" : " Beats");
    } else {
      return divisions[rack::math::clamp(val + 7, 0, 14)].label;
    }
  }
};

struct BendModule : rack::engine::Module {
  enum ParamIds {
    BEAT_DIV_PARAM,          // param 0 - beat division when in clocked mode
    LENGTH_PARAM,            // param 1 - bend length time when in fixed time mode
    RESET_PARAM,             // param 2 - manual soft reset button
    BEND_TRIG_PARAM,         // param 3 - manual bend trigger button
    BEND_ORIENTATION_PARAM,  // param 4 - bend orientation switch, towards or away from input signal
    BEND_DIR_PARAM,          // param 5 - bend direction switch, up or down
    BEND_AMOUNT_PARAM,       // param 6 - bend amount in semitones
    BEND_SHAPE_PARAM,        // param 7 - bend shape, log > lin > exp
    BEND_COMPLETION_PARAM,   // param 8 - bend completion behavior, hold or return
    BEND_TRACKING_PARAM,     // param 9 - bend input tracking behavior, continuous or sampled
    NUM_PARAMS
  };
  enum InputIds {
    EXT_CLOCK_INPUT,            // mono - external clock input
    BEAT_DIV_CV_INPUT,          // mono - CV for param 0
    LENGTH_CV_INPUT,            // mono - CV for param 1
    RESET_INPUT,                // mono - CV for param 2
    BEND_TRIG_INPUT,            // poly - bend trigger input
    SIGNALS_INPUT,              // poly - signals to be bent input
    BEND_ORIENTATION_CV_INPUT,  // mono - CV for param 4, zero = no change, negative = away, positive = towards
    BEND_DIR_CV_INPUT,          // mono - CV for param 5, zero = no change, negative = down, positive = up
    BEND_AMOUNT_CV_INPUT,       // mono - CV for param 6, V/Oct
    BEND_SHAPE_CV_INPUT,        // mono - CV for param 7
    BEND_COMPLETION_CV_INPUT,   // mono - CV for param 8, zero = no change, negative = return, positive = hold
    BEND_TRACKING_CV_INPUT,     // mono - CV for param 9, zero = no change, negative = sampled, positive = continuous
    NUM_INPUTS
  };
  enum OutputIds {
    SIGNALS_OUTPUT,  // poly - bent signals output
    NUM_OUTPUTS
  };
  enum LightIds {
    RESET_LIGHT,      // light for param 4
    BEND_TRIG_LIGHT,  // light for param 5
    NUM_LIGHTS
  };

  BendModule() {
    rack::engine::Module::config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

    /**
     * Clock-synced duration control with musical subdivision logic.
     * xV >= 0: Linear whole beats (Duration = V + 1). 0.0V = 1 Beat.
     * xV < 0: Discrete musical divisions via lookup table (-7.0V to -1.0V).
     * Mapped Divisions:
     * -7V: 1/32   - 32nd Note
     * -6V: 1/16T  - 16th Triplet
     * -5V: 1/16   - 16th Note
     * -4V: 1/8T   - 8th Triplet
     * -3V: 1/16.  - Dotted 16th Note
     * -2V: 1/8    - 8th Note
     * -1V: 1/8.   - Dotted 8th Note
     *  0V: 1/4    - Quarter Note (1 Beat)
     *  1V:	1/2	   - Half Note
     *  2V:	1/2.   - Dotted Half Note
     *  3V:	1/1    - Whole Note (1 Bar)
     *  4V:	5/4    - 5 Beats
     *  5V:	1/1.   - Dotted Whole Note (6 Beats)
     *  6V:	7/4    - 7 Beats
     *  7V:	2/1    - Double Whole Note (Breve / 2 Bars)
     */
    rack::engine::Module::configParam<BeatDivQuantity>(BEAT_DIV_PARAM, -7.0f, 7.0f, 0.0f, "Bend Duration (Clocked)");
    paramQuantities[BEAT_DIV_PARAM]->snapEnabled = true;
    rack::engine::Module::configParam(LENGTH_PARAM, 0.0f, 10.0f, 1.0f, "Bend Duration (Timed)", " Seconds");

    rack::engine::Module::configButton(RESET_PARAM, "Reset");
    rack::engine::Module::configButton(BEND_TRIG_PARAM, "Bend Trigger");

    rack::engine::Module::configSwitch(BEND_ORIENTATION_PARAM, DANT::BEND_DIR::TOWARDS_PITCH,
                                       DANT::BEND_DIR::AWAY_FROM_PITCH, DANT::BEND_DIR::TOWARDS_PITCH,
                                       "Bend Orientation", {"Towards Input", "Away from Input"});

    rack::engine::Module::configSwitch(BEND_DIR_PARAM, 0.0f, 1.0f, 1.0f, "Bend Direction", {"Down", "Up"});

    rack::engine::Module::configSwitch(BEND_COMPLETION_PARAM, 0.0f, 1.0f, 1.0f, "Bend Completion", {"Return", "Hold"});

    rack::engine::Module::configSwitch(BEND_TRACKING_PARAM, 0.0f, 1.0f, 1.0f, "Input Tracking",
                                       {"Sampled", "Continuous"});

    rack::engine::Module::configParam(BEND_AMOUNT_PARAM, 0.0f, 24.0f, 2.0f, "Bend Amount", " Semitones");
    paramQuantities[BEND_AMOUNT_PARAM]->snapEnabled = true;

    rack::engine::Module::configParam(BEND_SHAPE_PARAM, -1.0f, 1.0f, 0.0f, "Bend Shape", " (Log < Lin > Exp)");

    rack::engine::Module::configBypass(SIGNALS_INPUT, SIGNALS_OUTPUT);

    rack::engine::Module::configInput(EXT_CLOCK_INPUT, "External Clock");
    rack::engine::Module::configInput(BEAT_DIV_CV_INPUT, "[Poly] Bend Duration (Clocked) CV");
    rack::engine::Module::configInput(LENGTH_CV_INPUT, "[Poly] V/Sec Bend Duration (Timed) CV");
    rack::engine::Module::configInput(RESET_INPUT, "[Poly] Reset Trigger");
    rack::engine::Module::configInput(BEND_TRIG_INPUT, "[Poly] Bend Trigger");
    rack::engine::Module::configInput(SIGNALS_INPUT, "[Poly] V/Oct Signals");
    rack::engine::Module::configInput(BEND_ORIENTATION_CV_INPUT, "[Poly] Bend Orientation CV");
    rack::engine::Module::configInput(BEND_DIR_CV_INPUT, "[Poly] Bend Direction CV");
    rack::engine::Module::configInput(BEND_AMOUNT_CV_INPUT, "[Poly] V/Oct Bend Amount CV");
    rack::engine::Module::configInput(BEND_SHAPE_CV_INPUT, "[Poly] Bend Shape CV");
    rack::engine::Module::configInput(BEND_COMPLETION_CV_INPUT, "[Poly] Bend Completion CV");
    rack::engine::Module::configInput(BEND_TRACKING_CV_INPUT, "[Poly] Input Tracking CV");

    rack::engine::Module::configOutput(SIGNALS_OUTPUT, "[Poly] V/Oct Signals");
  }

  bool clockedMode{false};

  enum HoldMethod { INDEFINITE = 0, AUTO_UNHOLD = 1, GATE_BENDS = 2, TOGGLE_TRIGGERS = 3 };
  HoldMethod holdMethod{INDEFINITE};
  float autoUnholdThreshold{0.0f};  // V/Oct: 0.000833f (1 cent) to 0.08333f (100 cents)

  bool unbendEnvelope{false};
  bool inverseUnbendShape{false};
  float unbendDurationPct{0.10f};
  int gridLightChannels{0};
  rack::simd::float_4 gridLightValues[DANT::SIMD];

  float clockTimer{0.0f};
  float clockPeriod{0.0f};
  rack::dsp::SchmittTrigger clockTrigger;

  struct BendPolyState {
    rack::simd::float_4 active = rack::simd::float_4::zero();
    rack::simd::float_4 isUnbending = rack::simd::float_4::zero();
    rack::simd::float_4 startOffset = rack::simd::float_4::zero();
    rack::simd::float_4 targetOffset = rack::simd::float_4::zero();
    rack::simd::float_4 elapsedSeconds = rack::simd::float_4::zero();
    rack::simd::float_4 totalSeconds = {0.0f};
    rack::simd::float_4 sampledInputPitch = rack::simd::float_4::zero();
    rack::simd::float_4 isUp = rack::simd::float_4::zero();
  };

  BendPolyState bendStates[4];

  rack::dsp::SchmittTrigger resetTriggerDetectors[DANT::CHANS];
  rack::dsp::SchmittTrigger bendTriggerDetectors[DANT::CHANS];

  void softReset(int channel = -1) {
    if (channel == -1) {
      clockedMode = false;
      for (int i = 0; i < 4; ++i) {
        bendStates[i].active = rack::simd::float_4::zero();
        bendStates[i].isUnbending = rack::simd::float_4::zero();
        bendStates[i].startOffset = rack::simd::float_4::zero();
        bendStates[i].targetOffset = rack::simd::float_4::zero();
        bendStates[i].elapsedSeconds = rack::simd::float_4::zero();
        bendStates[i].totalSeconds = {0.0f};
        bendStates[i].sampledInputPitch = rack::simd::float_4::zero();
      }
    } else if (channel >= 0 && channel < DANT::CHANS) {
      int block = channel / 4;
      int lane = channel % 4;
      bendStates[block].active[lane] = 0.0f;
      bendStates[block].isUnbending[lane] = 0.0f;
      bendStates[block].startOffset[lane] = 0.0f;
      bendStates[block].targetOffset[lane] = 0.0f;
      bendStates[block].elapsedSeconds[lane] = 0.0f;
      bendStates[block].totalSeconds[lane] = 0.0f;
      bendStates[block].sampledInputPitch[lane] = 0.0f;
      bendStates[block].isUp[lane] = 0.0f;  // Reset isUp
    }
  }

  void onReset() override {
    softReset();
    unbendEnvelope = false;
    inverseUnbendShape = false;
    unbendDurationPct = 0.10f;
    holdMethod = INDEFINITE;
    autoUnholdThreshold = 0.0f;
    for (int c = 0; c < DANT::CHANS; ++c) {
      resetTriggerDetectors[c].reset();
    }
  }

  json_t* dataToJson() override {
    json_t* rootJ = json_object();
    json_object_set_new(rootJ, "unbendEnvelope", json_boolean(unbendEnvelope));
    json_object_set_new(rootJ, "inverseUnbendShape", json_boolean(inverseUnbendShape));
    json_object_set_new(rootJ, "unbendDurationPct", json_real(static_cast<double>(unbendDurationPct)));
    json_object_set_new(rootJ, "holdMethod", json_integer(static_cast<int>(holdMethod)));
    json_object_set_new(rootJ, "autoUnholdThreshold", json_real(static_cast<double>(autoUnholdThreshold)));
    return rootJ;
  }

  void dataFromJson(json_t* rootJ) override {
    if (json_t* j = json_object_get(rootJ, "unbendEnvelope")) unbendEnvelope = json_boolean_value(j);
    if (json_t* j = json_object_get(rootJ, "inverseUnbendShape")) inverseUnbendShape = json_boolean_value(j);
    if (json_t* j = json_object_get(rootJ, "unbendDurationPct"))
      unbendDurationPct = static_cast<float>(json_real_value(j));
    if (json_t* j = json_object_get(rootJ, "holdMethod")) holdMethod = static_cast<HoldMethod>(json_integer_value(j));
    if (json_t* j = json_object_get(rootJ, "autoUnholdThreshold"))
      autoUnholdThreshold = static_cast<float>(json_real_value(j));
  }

  void process(const rack::engine::Module::ProcessArgs& args) override {
    int numChannels = inputs[SIGNALS_INPUT].getChannels();
    gridLightChannels = numChannels;

    processResets();

    if (numChannels > 0) {
      processClock(args.sampleTime);

      for (int c{0}; c < numChannels; ++c) {
        if (processBendTriggers(c)) {
          triggerBend(c);
        }
      }

      for (int c{0}; c < numChannels; c += DANT::SIMD) {
        rack::simd::float_4 inputSignals = inputs[SIGNALS_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);

        DANT::BendOpts opts;
        int block = c / 4;

        float validLanes[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        for (int i = 0; i < DANT::SIMD; ++i) {
          if (c + i < numChannels) validLanes[i] = 1.0f;
        }
        rack::simd::float_4 validMask = rack::simd::float_4::load(validLanes) > 0.5f;

        rack::simd::float_4 activeMask = (bendStates[block].active != 0.0f) & validMask;

        if (rack::simd::movemask(activeMask) != 0) {
          // Track sampled or live inputs via SIMD
          rack::simd::float_4 trackCV =
              inputs[BEND_TRACKING_CV_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
          rack::simd::float_4 trackKnob = params[BEND_TRACKING_PARAM].getValue();
          rack::simd::float_4 isSampledMask = (trackCV < 0.0f) | ((trackCV == 0.0f) & (trackKnob < 0.5f));
          rack::simd::float_4 shouldSampleMask = isSampledMask & activeMask;

          inputSignals = rack::simd::ifelse(shouldSampleMask, bendStates[block].sampledInputPitch, inputSignals);

          // Calculate shape via SIMD
          rack::simd::float_4 shapeCV =
              inputs[BEND_SHAPE_CV_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
          rack::simd::float_4 shapeKnob = params[BEND_SHAPE_PARAM].getValue();
          opts.shape = rack::simd::clamp(shapeKnob + shapeCV, -1.0f, 1.0f);

          // Elapsed time
          bendStates[block].elapsedSeconds = rack::simd::ifelse(
              activeMask, bendStates[block].elapsedSeconds + args.sampleTime, bendStates[block].elapsedSeconds);

          rack::simd::float_4 maskTotalPos = bendStates[block].totalSeconds > 0.0f;
          rack::simd::float_4 prog =
              rack::simd::ifelse(maskTotalPos, bendStates[block].elapsedSeconds / bendStates[block].totalSeconds, 1.0f);

          // readBendCompletion vectorially
          rack::simd::float_4 compCV =
              inputs[BEND_COMPLETION_CV_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
          rack::simd::float_4 paramComp = params[BEND_COMPLETION_PARAM].getValue();
          rack::simd::float_4 maskIsHold = (compCV > 0.0f) | ((compCV == 0.0f) & (paramComp > 0.5f));

          rack::simd::float_4 wantsUnholdMask = rack::simd::float_4::zero();

          if (holdMethod == GATE_BENDS) {
            rack::simd::float_4 trigIn =
                inputs[BEND_TRIG_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c) +
                params[BEND_TRIG_PARAM].getValue();
            wantsUnholdMask = trigIn <= 0.0f;
          } else {
            rack::simd::float_4 progFinishedMask = prog >= 1.0f;
            rack::simd::float_4 isReturnMask = maskIsHold == 0.0f;
            wantsUnholdMask = wantsUnholdMask | (progFinishedMask & isReturnMask);

            if (holdMethod == AUTO_UNHOLD) {
              rack::simd::float_4 currentInput =
                  inputs[SIGNALS_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
              rack::simd::float_4 pitchDiffAbs = rack::simd::abs(currentInput - bendStates[block].sampledInputPitch);
              rack::simd::float_4 diffExceededMask = pitchDiffAbs > autoUnholdThreshold;
              wantsUnholdMask = wantsUnholdMask | (progFinishedMask & diffExceededMask);
            }
          }

          rack::simd::float_4 isNotUnbendingMask = bendStates[block].isUnbending == 0.0f;
          rack::simd::float_4 triggerUnholdMask = activeMask & wantsUnholdMask & isNotUnbendingMask;

          if (rack::simd::movemask(triggerUnholdMask) != 0) {
            rack::simd::float_4 clampedProg = rack::simd::fmax(0.0f, prog);
            rack::simd::float_4 exp = rack::dsp::exp2_taylor5(opts.shape * 2.0f);
            rack::simd::float_4 curved =
                rack::simd::ifelse(clampedProg > 0.0f, rack::simd::pow(clampedProg, exp), 0.0f);

            rack::simd::float_4 currentOffset =
                bendStates[block].startOffset +
                (bendStates[block].targetOffset - bendStates[block].startOffset) * curved;
            currentOffset = rack::simd::ifelse(prog >= 1.0f, bendStates[block].targetOffset, currentOffset);

            if (unbendEnvelope) {
              bendStates[block].isUnbending =
                  rack::simd::ifelse(triggerUnholdMask, 1.0f, bendStates[block].isUnbending);
              bendStates[block].startOffset =
                  rack::simd::ifelse(triggerUnholdMask, currentOffset, bendStates[block].startOffset);
              bendStates[block].targetOffset =
                  rack::simd::ifelse(triggerUnholdMask, 0.0f, bendStates[block].targetOffset);
              bendStates[block].elapsedSeconds =
                  rack::simd::ifelse(triggerUnholdMask, 0.0f, bendStates[block].elapsedSeconds);
              bendStates[block].totalSeconds = rack::simd::ifelse(
                  triggerUnholdMask, rack::simd::fmax(0.0f, bendStates[block].totalSeconds * unbendDurationPct),
                  bendStates[block].totalSeconds);
              prog = rack::simd::ifelse(triggerUnholdMask, 0.0f, prog);
            } else {
              bendStates[block].active = rack::simd::ifelse(triggerUnholdMask, 0.0f, bendStates[block].active);
              prog = rack::simd::ifelse(triggerUnholdMask, 0.0f, prog);
              bendStates[block].startOffset =
                  rack::simd::ifelse(triggerUnholdMask, 0.0f, bendStates[block].startOffset);
              bendStates[block].targetOffset =
                  rack::simd::ifelse(triggerUnholdMask, 0.0f, bendStates[block].targetOffset);
              rack::simd::float_4 rawInputs =
                  inputs[SIGNALS_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
              inputSignals = rack::simd::ifelse(triggerUnholdMask, rawInputs, inputSignals);
            }
          }

          rack::simd::float_4 isUnbendingMask = bendStates[block].isUnbending != 0.0f;
          rack::simd::float_4 triggerFinishUnbendMask = activeMask & (prog >= 1.0f) & isUnbendingMask;

          if (rack::simd::movemask(triggerFinishUnbendMask) != 0) {
            bendStates[block].active = rack::simd::ifelse(triggerFinishUnbendMask, 0.0f, bendStates[block].active);
            bendStates[block].isUnbending =
                rack::simd::ifelse(triggerFinishUnbendMask, 0.0f, bendStates[block].isUnbending);
            prog = rack::simd::ifelse(triggerFinishUnbendMask, 0.0f, prog);
            bendStates[block].startOffset =
                rack::simd::ifelse(triggerFinishUnbendMask, 0.0f, bendStates[block].startOffset);
            bendStates[block].targetOffset =
                rack::simd::ifelse(triggerFinishUnbendMask, 0.0f, bendStates[block].targetOffset);

            rack::simd::float_4 rawInputs =
                inputs[SIGNALS_INPUT].getNormalPolyVoltageSimd<rack::simd::float_4>(0.0f, c);
            inputSignals = rack::simd::ifelse(triggerFinishUnbendMask, rawInputs, inputSignals);
          }

          prog = rack::simd::ifelse(prog > 1.0f, 1.0f, prog);

          opts.startOffsets = bendStates[block].startOffset;
          opts.targetOffsets = bendStates[block].targetOffset;
          opts.progress = prog;
          opts.isUnbending = bendStates[block].isUnbending;
          opts.inverseUnbend = inverseUnbendShape;

          activeMask = (bendStates[block].active != 0.0f) & validMask;

          rack::simd::float_4 intensity = rack::simd::fmin(1.0f, prog);
          rack::simd::float_4 isCurrentlyUnbending = bendStates[block].isUnbending != 0.0f;
          intensity = rack::simd::ifelse(isCurrentlyUnbending, 1.0f - intensity, intensity);
          intensity = rack::simd::ifelse(prog >= 1.0f, 1.0f, intensity);

          rack::simd::float_4 finalIntensity = rack::simd::ifelse(activeMask, intensity * bendStates[block].isUp, 0.0f);
          gridLightValues[block] = finalIntensity;

        } else {
          gridLightValues[block] = rack::simd::float_4::zero();
        }

        rack::simd::float_4 outSignals = DANT::bendVoct(inputSignals, opts);
        outputs[SIGNALS_OUTPUT].setVoltageSimd<rack::simd::float_4>(outSignals, c);
      }

      outputs[SIGNALS_OUTPUT].setChannels(numChannels);
    }

    bool resetActive = params[RESET_PARAM].getValue() > 0.0f;
    for (int c = 0; c < std::max(1, inputs[RESET_INPUT].getChannels()); ++c) {
      if (inputs[RESET_INPUT].getPolyVoltage(c) > 0.0f) {
        resetActive = true;
        break;
      }
    }
    lights[RESET_LIGHT].setSmoothBrightness(resetActive ? 1.0f : 0.0f, args.sampleTime);

    bool bendTrigActive = params[BEND_TRIG_PARAM].getValue() > 0.0f;
    for (int c = 0; c < std::max(1, inputs[BEND_TRIG_INPUT].getChannels()); ++c) {
      if (inputs[BEND_TRIG_INPUT].getPolyVoltage(c) > 0.0f) {
        bendTrigActive = true;
        break;
      }
    }
    lights[BEND_TRIG_LIGHT].setSmoothBrightness(bendTrigActive ? 1.0f : 0.0f, args.sampleTime);
  }

  inline void processResets() {
    bool manualReset = params[RESET_PARAM].getValue() > 0.0f;
    bool globalResetTrig = false;

    int resetChannels = inputs[RESET_INPUT].getChannels();
    for (int c = 0; c < DANT::CHANS; ++c) {
      float resetIn = inputs[RESET_INPUT].getNormalPolyVoltage(0.0f, c);
      if (resetTriggerDetectors[c].process(resetIn)) {
        if (resetChannels <= 1 && c == 0) {
          globalResetTrig = true;
        } else if (resetChannels > 1) {
          softReset(c);
        }
      }
    }

    if (manualReset || globalResetTrig) {
      softReset(-1);
    }
  }

  inline void processClock(float sampleTime) {
    clockTimer += sampleTime;

    if (inputs[EXT_CLOCK_INPUT].isConnected()) {
      clockedMode = true;
      if (clockTrigger.process(inputs[EXT_CLOCK_INPUT].getVoltage())) {
        clockPeriod = clockTimer;
        clockTimer = 0.0f;
      }
    } else {
      clockedMode = false;
      clockPeriod = 0.0f;
    }
  }

  inline DANT::BEND_DIR readBendOrientation(int channel) {
    float cvInput = inputs[BEND_ORIENTATION_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    if (cvInput < 0.0f) return DANT::BEND_DIR::AWAY_FROM_PITCH;
    if (cvInput > 0.0f) return DANT::BEND_DIR::TOWARDS_PITCH;

    return params[BEND_ORIENTATION_PARAM].getValue() > 0.5f ? DANT::BEND_DIR::AWAY_FROM_PITCH
                                                            : DANT::BEND_DIR::TOWARDS_PITCH;
  }

  inline bool processBendTriggers(int channel) {
    int block = channel / 4;
    int lane = channel % 4;
    float trigIn = readBendTrigger(channel);
    bool triggerFired = false;

    if (holdMethod == GATE_BENDS) {
      triggerFired = bendTriggerDetectors[channel].process(trigIn);
    } else {
      triggerFired = bendTriggerDetectors[channel].process(trigIn);

      if (triggerFired && holdMethod == TOGGLE_TRIGGERS && readBendCompletion(channel)) {
        if (bendStates[block].active[lane] != 0.0f && bendStates[block].isUnbending[lane] == 0.0f) {
          float prog = 1.0f;
          if (bendStates[block].totalSeconds[lane] > 0.0f) {
            prog = bendStates[block].elapsedSeconds[lane] / bendStates[block].totalSeconds[lane];
          }
          float currentOffset = bendStates[block].targetOffset[lane];

          if (prog < 1.0f) {
            float shape = rack::math::clamp(readBendShape(channel), -1.0f, 1.0f);
            float exp = rack::dsp::exp2_taylor5(shape * 2.0f);
            float curved = prog > 0.0f ? std::pow(prog, exp) : 0.0f;
            currentOffset = bendStates[block].startOffset[lane] +
                            (bendStates[block].targetOffset[lane] - bendStates[block].startOffset[lane]) * curved;
          }

          if (unbendEnvelope) {
            bendStates[block].isUnbending[lane] = 1.0f;
            bendStates[block].startOffset[lane] = currentOffset;
            bendStates[block].targetOffset[lane] = 0.0f;
            bendStates[block].elapsedSeconds[lane] = 0.0f;
            bendStates[block].totalSeconds[lane] =
                std::fmax(0.0f, bendStates[block].totalSeconds[lane] * unbendDurationPct);
          } else {
            bendStates[block].active[lane] = 0.0f;
          }
          triggerFired = false;
        }
      }
    }
    return triggerFired;
  }

  inline float readBendTrigger(int channel) {
    return inputs[BEND_TRIG_INPUT].getNormalPolyVoltage(0.0f, channel) + params[BEND_TRIG_PARAM].getValue();
  }

  inline bool readBendCompletion(int channel) {
    float compCV = inputs[BEND_COMPLETION_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    if (compCV < 0.0f) return false;
    if (compCV > 0.0f) return true;
    return params[BEND_COMPLETION_PARAM].getValue() > 0.5f;
  }

  inline void triggerBend(int channel) {
    int block = channel / 4;
    int lane = channel % 4;

    float amountVolts = readBendAmount(channel) / 12.0f;

    float duration = 0.01f;
    if (clockedMode && clockPeriod > 0.0f) {
      duration = readBendDurationClocked(channel, clockPeriod);
    } else {
      duration = readBendDurationTimed(channel);
    }

    bool isUp = readBendDirection(channel);

    bendStates[block].active[lane] = 1.0f;
    bendStates[block].isUnbending[lane] = 0.0f;
    bendStates[block].elapsedSeconds[lane] = 0.0f;
    bendStates[block].totalSeconds[lane] = duration;
    bendStates[block].isUp[lane] = isUp ? 1.0f : -1.0f;

    DANT::BEND_DIR bendDir = readBendOrientation(channel);
    if (bendDir == DANT::BEND_DIR::AWAY_FROM_PITCH) {
      bendStates[block].startOffset[lane] = 0.0f;
      bendStates[block].targetOffset[lane] = isUp ? amountVolts : -amountVolts;
    } else {
      bendStates[block].startOffset[lane] = isUp ? -amountVolts : amountVolts;
      bendStates[block].targetOffset[lane] = 0.0f;
    }

    // We must always record the sampled pitch when triggering a bend,
    // because AUTO_UNHOLD uses this as its baseline reference regardless
    // of whether Continuous Tracking is enabled for the bend mechanics.
    bendStates[block].sampledInputPitch[lane] = inputs[SIGNALS_INPUT].getNormalPolyVoltage(0.0f, channel);
  }

  inline float readBendAmount(int channel) {
    float amountCV = inputs[BEND_AMOUNT_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    float amountSemitones = params[BEND_AMOUNT_PARAM].getValue() + (amountCV * 12.0f);
    return std::fmax(0.0f, amountSemitones);
  }

  inline float readBendDurationClocked(int channel, float clockPeriod) {
    float beatDivCv = inputs[BEAT_DIV_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    float valF = params[BEAT_DIV_PARAM].getValue() + beatDivCv;
    int val = static_cast<int>(valF + (valF > 0.0f ? 0.5f : -0.5f));
    float multiplier = 1.0f;
    if (val >= 0) {
      multiplier = val + 1.0f;
    } else {
      int index = std::max(0, val + 7);
      multiplier = divisions[index].multiplier;
    }
    return std::fmax(0.0f, clockPeriod * multiplier);
  }

  inline float readBendDurationTimed(int channel) {
    float lengthVolts = params[LENGTH_PARAM].getValue() + inputs[LENGTH_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    return std::fmax(0.0f, lengthVolts);
  }

  inline bool readBendDirection(int channel) {
    float dirCV = inputs[BEND_DIR_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    if (dirCV < 0.0f) return false;
    if (dirCV > 0.0f) return true;
    return params[BEND_DIR_PARAM].getValue() > 0.5f;
  }

  inline bool readBendTracking(int channel) {
    float trackCV = inputs[BEND_TRACKING_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
    if (trackCV < 0.0f) return true;
    if (trackCV > 0.0f) return false;
    return params[BEND_TRACKING_PARAM].getValue() < 0.5f;
  }

  inline float readBendShape(int channel) {
    return params[BEND_SHAPE_PARAM].getValue() + inputs[BEND_SHAPE_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
  }
};

static const std::string RESET_ARROW{"\uf56c"};
static const std::string EXT_CLOCK{"\uf381"};
static const std::string LENGTH_WATCH{"\uf2ca"};
static const std::string BEND_DIR_UP{"\ue5d8"};
static const std::string BEND_DIR_DOWN{"\ue5db"};
static const std::string BEND_TRIG{"\uf4b4"};

struct BendWidget : DANT::ModuleWidget {
  std::string moduleName() override { return "Bend"; }

  rack::componentlibrary::VCVLightButton<rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::RedLight>>*
      resetButton;
  DANT::Port* resetInputPort;

  DANT::Knob* beatDivKnob;
  DANT::Port* extClockInputPort;
  DANT::Port* beatDivCvInputPort;
  DANT::Knob* lengthKnob;
  DANT::Port* lengthCvInputPort;

  rack::componentlibrary::CKSS* bendOrientationSwitch;
  DANT::Port* bendOrientationCvInputPort;

  rack::componentlibrary::CKSS* bendDirSwitch;
  DANT::Port* bendDirCvInputPort;

  DANT::Knob* bendAmountKnob;
  DANT::Port* bendAmountCvInputPort;

  DANT::Knob* bendShapeKnob;
  DANT::Port* bendShapeCvInputPort;

  rack::componentlibrary::CKSS* bendCompletionSwitch;
  DANT::Port* bendCompletionCvInputPort;

  rack::componentlibrary::CKSS* bendTrackingSwitch;
  DANT::Port* bendTrackingCvInputPort;

  rack::componentlibrary::VCVLightButton<
      rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::YellowLight>>* bendTrigButton;
  DANT::Port* bendTrigInputPort;
  DANT::GridLight* bendGridLight;

  DANT::Port* signalsInputPort;
  DANT::Port* signalsOutputPort;

  BendWidget(BendModule* module) {
    rack::app::ModuleWidget::setModule(module);
    this->box.size = rack::math::Vec(rack::app::RACK_GRID_WIDTH * HP, rack::app::RACK_GRID_HEIGHT);

    resetButton = rack::createLightParamCentered<rack::componentlibrary::VCVLightButton<
        rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::RedLight>>>(
        DANT::layout(7.0f, 1.0f), module, BendModule::RESET_PARAM, BendModule::RESET_LIGHT);
    resetInputPort = rack::createInputCentered<DANT::Port>(DANT::layout(7.0f, 2.0f), module, BendModule::RESET_INPUT);

    beatDivKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(2.0f, 4.0f), module, BendModule::BEAT_DIV_PARAM);
    beatDivKnob->vizType = DANT::KnobViz::NOTCHES;
    beatDivKnob->numNotches = 15;
    extClockInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.0f, 4.0f), module, BendModule::EXT_CLOCK_INPUT);
    beatDivCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 5.0f), module, BendModule::BEAT_DIV_CV_INPUT);

    lengthKnob = rack::createParamCentered<DANT::Knob>(DANT::layout(7.0f, 4.0f), module, BendModule::LENGTH_PARAM);
    lengthKnob->vizType = DANT::KnobViz::UNIARC;
    lengthKnob->numNotches = 11;
    lengthKnob->inputId = BendModule::LENGTH_CV_INPUT;
    lengthCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(7.0f, 5.0f), module, BendModule::LENGTH_CV_INPUT);

    // The 3 switches in a horizontal line at Y=6.7
    // Left (Orientation)
    bendOrientationSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(2.0f, 6.7f), module,
                                                                                    BendModule::BEND_ORIENTATION_PARAM);
    bendOrientationCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 8.3f), module, BendModule::BEND_ORIENTATION_CV_INPUT);

    // Center (Direction)
    bendDirSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(4.5f, 6.7f), module,
                                                                            BendModule::BEND_DIR_PARAM);
    bendDirCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.5f, 8.3f), module, BendModule::BEND_DIR_CV_INPUT);

    // Right (Completion)
    bendCompletionSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(7.0f, 6.7f), module,
                                                                                   BendModule::BEND_COMPLETION_PARAM);
    bendCompletionCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(7.0f, 8.3f), module, BendModule::BEND_COMPLETION_CV_INPUT);

    // Tracking switch in the center (X=4.5), moved up to Y=9.9
    bendTrackingSwitch = rack::createParamCentered<rack::componentlibrary::CKSS>(DANT::layout(4.5f, 9.9f), module,
                                                                                 BendModule::BEND_TRACKING_PARAM);
    bendTrackingCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.5f, 11.5f), module, BendModule::BEND_TRACKING_CV_INPUT);

    // Knobs reverted to original positions but shifted down to Y=10.4
    bendAmountKnob =
        rack::createParamCentered<DANT::Knob>(DANT::layout(2.0f, 10.4f), module, BendModule::BEND_AMOUNT_PARAM);
    bendAmountKnob->vizType = DANT::KnobViz::UNIARC;
    bendAmountKnob->numNotches = 5;
    bendAmountKnob->inputId = BendModule::BEND_AMOUNT_CV_INPUT;
    bendAmountCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 11.9f), module, BendModule::BEND_AMOUNT_CV_INPUT);

    bendShapeKnob =
        rack::createParamCentered<DANT::Knob>(DANT::layout(7.0f, 10.4f), module, BendModule::BEND_SHAPE_PARAM);
    bendShapeKnob->vizType = DANT::KnobViz::BIPARC;
    bendShapeKnob->inputId = BendModule::BEND_SHAPE_CV_INPUT;
    bendShapeCvInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(7.0f, 11.9f), module, BendModule::BEND_SHAPE_CV_INPUT);

    bendTrigButton = rack::createLightParamCentered<rack::componentlibrary::VCVLightButton<
        rack::componentlibrary::MediumSimpleLight<rack::componentlibrary::YellowLight>>>(
        DANT::layout(4.5f, 13.5f), module, BendModule::BEND_TRIG_PARAM, BendModule::BEND_TRIG_LIGHT);
    bendTrigInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(4.5f, 14.5f), module, BendModule::BEND_TRIG_INPUT);

    bendGridLight = rack::createWidgetCentered<DANT::GridLight>(DANT::layout(4.5f, 15.65f));
    bendGridLight->oneMode();  // Values range from -1.0 to 1.0 continuously
    if (module) {
      bendGridLight->numChannels = &module->gridLightChannels;
      bendGridLight->channelValues = module->gridLightValues;
    }

    signalsInputPort =
        rack::createInputCentered<DANT::Port>(DANT::layout(2.0f, 15.0f), module, BendModule::SIGNALS_INPUT);
    signalsOutputPort =
        rack::createOutputCentered<DANT::Port>(DANT::layout(7.0f, 15.0f), module, BendModule::SIGNALS_OUTPUT);
    signalsOutputPort->isOutput = true;

    rack::app::ModuleWidget::addParam(resetButton);
    rack::app::ModuleWidget::addInput(resetInputPort);

    rack::app::ModuleWidget::addInput(extClockInputPort);

    rack::app::ModuleWidget::addParam(beatDivKnob);
    rack::app::ModuleWidget::addInput(beatDivCvInputPort);
    rack::app::ModuleWidget::addParam(lengthKnob);
    rack::app::ModuleWidget::addInput(lengthCvInputPort);

    rack::app::ModuleWidget::addParam(bendOrientationSwitch);
    rack::app::ModuleWidget::addInput(bendOrientationCvInputPort);

    rack::app::ModuleWidget::addParam(bendDirSwitch);
    rack::app::ModuleWidget::addInput(bendDirCvInputPort);

    rack::app::ModuleWidget::addParam(bendAmountKnob);
    rack::app::ModuleWidget::addInput(bendAmountCvInputPort);

    rack::app::ModuleWidget::addParam(bendShapeKnob);
    rack::app::ModuleWidget::addInput(bendShapeCvInputPort);

    rack::app::ModuleWidget::addParam(bendCompletionSwitch);
    rack::app::ModuleWidget::addInput(bendCompletionCvInputPort);

    rack::app::ModuleWidget::addParam(bendTrackingSwitch);
    rack::app::ModuleWidget::addInput(bendTrackingCvInputPort);

    rack::app::ModuleWidget::addParam(bendTrigButton);
    rack::app::ModuleWidget::addInput(bendTrigInputPort);
    rack::app::ModuleWidget::addChild(bendGridLight);

    rack::app::ModuleWidget::addInput(signalsInputPort);
    rack::app::ModuleWidget::addOutput(signalsOutputPort);
  }

  void draw(const rack::widget::Widget::DrawArgs& args) override {
    DANT::ModuleWidget::draw(args);
    DANT::Fonts::DrawOptions opts;
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;
    opts.size = 20.0f;
    opts.xpos = DANT::layout(5.5f, 1.5f).x;
    opts.ypos = DANT::layout(5.5f, 1.5f).y;
    DANT::Fonts::drawSymbols(args, RESET_ARROW, opts);
    opts.xpos = DANT::layout(4.0f, 5.0f).x;
    opts.ypos = DANT::layout(4.0f, 5.0f).y;
    DANT::Fonts::drawSymbols(args, EXT_CLOCK, opts);
    opts.xpos = DANT::layout(5.5f, 5.0f).x;
    opts.ypos = DANT::layout(5.5f, 5.0f).y;
    DANT::Fonts::drawSymbols(args, LENGTH_WATCH, opts);

    // Core Layout Symbols
    opts.xpos = DANT::layout(4.5f, 12.75f).x;
    opts.ypos = DANT::layout(4.5f, 12.75f).y;
    DANT::Fonts::drawSymbols(args, BEND_TRIG, opts);
    opts.xpos = DANT::layout(2.0f, 14.0f).x;
    opts.ypos = DANT::layout(2.0f, 14.0f).y;
    DANT::Fonts::drawSymbols(args, DANT::INPUT_CIRCLE, opts);
    opts.xpos = DANT::layout(7.0f, 14.0f).x;
    opts.ypos = DANT::layout(7.0f, 14.0f).y;
    DANT::Fonts::drawSymbols(args, DANT::OUTPUT_CIRCLE, opts);
    // Text positioning aligned directly to the switches based on the layout
    opts.ttfFile = DANT::REGULAR_TTF;
    opts.size = 9.5f;
    opts.align = NVG_ALIGN_MIDDLE | NVG_ALIGN_CENTER;

    // Orientation (Left 2.0, 6.7)
    opts.xpos = DANT::layout(2.0f, 6.0f).x;
    opts.ypos = DANT::layout(2.0f, 6.0f).y;
    DANT::Fonts::drawText(args, "Away", opts);
    opts.xpos = DANT::layout(2.0f, 7.4f).x;
    opts.ypos = DANT::layout(2.0f, 7.4f).y;
    DANT::Fonts::drawText(args, "Toward", opts);

    // Direction (Center 4.5, 6.7) - Material Symbols Sharp
    opts.xpos = DANT::layout(4.5f, 6.0f).x;
    opts.ypos = DANT::layout(4.5f, 6.0f).y;
    DANT::Fonts::drawSymbols(args, BEND_DIR_UP, opts);
    opts.xpos = DANT::layout(4.5f, 7.4f).x;
    opts.ypos = DANT::layout(4.5f, 7.4f).y;
    DANT::Fonts::drawSymbols(args, BEND_DIR_DOWN, opts);

    // Completion (Right 7.0, 6.7)
    opts.xpos = DANT::layout(7.0f, 6.0f).x;
    opts.ypos = DANT::layout(7.0f, 6.0f).y;
    DANT::Fonts::drawText(args, "Hold", opts);
    opts.xpos = DANT::layout(7.0f, 7.4f).x;
    opts.ypos = DANT::layout(7.0f, 7.4f).y;
    DANT::Fonts::drawText(args, "Rtrn", opts);

    // Tracking (Center 4.5, 9.9)
    opts.xpos = DANT::layout(4.5f, 9.2f).x;
    opts.ypos = DANT::layout(4.5f, 9.2f).y;
    DANT::Fonts::drawText(args, "Cont", opts);
    opts.xpos = DANT::layout(4.5f, 10.6f).x;
    opts.ypos = DANT::layout(4.5f, 10.6f).y;
    DANT::Fonts::drawText(args, "Smpl", opts);

    // Knobs Labels (Y: 10.4) -> Place labels directly above at Y: 9.2
    opts.size = 11.0f;
    opts.xpos = DANT::layout(2.0f, 9.2f).x;
    opts.ypos = DANT::layout(2.0f, 9.2f).y;
    DANT::Fonts::drawText(args, "Amount", opts);

    opts.xpos = DANT::layout(7.0f, 9.2f).x;
    opts.ypos = DANT::layout(7.0f, 9.2f).y;
    DANT::Fonts::drawText(args, "Shape", opts);
  }

  void appendContextMenu(rack::ui::Menu* menu) override {
    DANT::ModuleWidget::appendContextMenu(menu);

    BendModule* module = dynamic_cast<BendModule*>(this->module);
    if (!module) return;

    menu->addChild(new rack::ui::MenuSeparator);

    // Unbend Sub-menu
    menu->addChild(rack::createSubmenuItem("Unbend", "", [=](rack::ui::Menu* menu) {
      // Unbend Envelope Toggle
      menu->addChild(rack::createBoolPtrMenuItem("Unbend Envelope", "", &module->unbendEnvelope));

      // Inverse Shape Toggle
      menu->addChild(rack::createBoolPtrMenuItem("Inverse Shape", "", &module->inverseUnbendShape));

      // Unbend Duration Slider
      auto* durSlider =
          new DANT::MenuSlider(new DANT::FloatValueQuantity("Unbend Duration", 0.0f, 2.0f, 0.10f,
                                                            &module->unbendDurationPct, "%", 100.0f, "%.0f"),
                               DANT::RGB_SLIDER_WIDTH);
      menu->addChild(durSlider);
    }));

    // Hold Method Sub-menu
    menu->addChild(rack::createSubmenuItem("Hold Method", "", [=](rack::ui::Menu* menu) {
      auto addHoldMethodItem = [=](const std::string& name, BendModule::HoldMethod method) {
        menu->addChild(rack::createMenuItem(name, module->holdMethod == method ? "✔" : "",
                                            [=]() { module->holdMethod = method; }));
      };

      addHoldMethodItem("Indefinite", BendModule::INDEFINITE);
      addHoldMethodItem("Auto-Unhold", BendModule::AUTO_UNHOLD);

      auto* threshSlider =
          new DANT::MenuSlider(new DANT::FloatValueQuantity("Threshold", 0.000833333f, 0.08333333f, 0.0f,
                                                            &module->autoUnholdThreshold, " cents", 1200.0f, "%.0f"),
                               DANT::RGB_SLIDER_WIDTH);
      menu->addChild(threshSlider);

      addHoldMethodItem("Gate-Bends", BendModule::GATE_BENDS);
      addHoldMethodItem("Toggle Triggers", BendModule::TOGGLE_TRIGGERS);
    }));
  }
};

rack::plugin::Model* modelBend = rack::createModel<BendModule, BendWidget>("Bend");
