#pragma once

#include <algorithm>  // std::fill std::min std::max
#include <cmath>      // std::fabs

#include "../static.hpp"

namespace DANT {

static const int POLY_CYCLE_MAX_DELAY{64};  // samples
static const int POLY_CYCLE_HISTORY{POLY_CYCLE_MAX_DELAY + 1};
// Stands for "no such event", longer than any window and small enough to count up from without overflowing.
static const int POLY_CYCLE_LONG_AGO{1 << 20};
// Adjacent notes are a semitone apart only to within floating point error, without this a threshold of exactly one
// semitone would miss some of them.
static const float POLY_CYCLE_CHANGE_TOLERANCE{0.001f};
static const float POLY_CYCLE_GATE_HIGH{10.0f};
// A receiving module only sees a new rising edge once the gate has been at or below this.
static const float POLY_CYCLE_GATE_LOW{0.1f};
// A step is a change of at least STEP_MIN that follows FLAT_SAMPLES of no movement. Timing is only measured on steps,
// a signal that is always moving has no single moment to line up with its trigger.
static const float POLY_CYCLE_FLAT_EPSILON{0.0001f};
static const float POLY_CYCLE_STEP_MIN{0.001f};
static const int POLY_CYCLE_FLAT_SAMPLES{4};

enum IDLE_MODE { IDLE_HOLD, IDLE_ZERO };

enum GATE_MODE { GATE_PASS, GATE_OFF, GATE_PULSE };

struct PolyCycleOpts {
  int channels{1};                      // number of output channels to cycle through
  IDLE_MODE idleMode{IDLE_HOLD};        // what the channels that are not receiving the signal output
  bool sampleAndHold{false};            // take one sample when a note starts instead of following the signal
  bool resetClears{false};              // a reset also sets every channel of both outputs to zero
  bool autoAlign{false};                // measure how far apart the signal and its trigger arrive, and correct it
  int delaySamples{0};                  // fixed signal delay, used when autoAlign is off
  bool advanceOnChange{false};          // also advance when the signal jumps
  float changeThreshold{1.0f / 12.0f};  // size of jump that advances, in volts
  float followCoeff{0.02f};             // how quickly slow movement of the signal is absorbed, per sample
  int windowSamples{48};                // events this close together belong to the same note
  GATE_MODE gateMode{GATE_PASS};        // pass the trigger voltage through, or make a pulse for each note
  int gateSamples{48};                  // length of a generated pulse

  PolyCycleOpts() = default;
};

/**
 * Routes a mono signal to one channel of a poly output, moving to the next channel on each trigger.
 * The trigger voltage is routed alongside it, on the same channel, or replaced by a pulse made for each note.
 *
 * A "note" starts each time the signal moves to the next channel. The first note after start up or a reset stays on
 * the first channel.
 */
struct PolyCycle {
  enum Source { SOURCE_NONE, SOURCE_TRIGGER, SOURCE_CHANGE };

  // The inputs after timing correction, as the rest of the code sees them.
  struct Inputs {
    float signal;
    float trigVoltage;
    bool trigger;
  };

  /**
   * Results, read these after calling step.
   */
  float outs[DANT::CHANS]{};
  float gateOuts[DANT::CHANS]{};
  int active{0};  // channel currently receiving the signal
  int skew{0};    // samples the trigger arrives before the signal, negative if it arrives after

  /**
   * Channel state.
   */
  // The next note keeps the active channel instead of moving on, so that it lands on the first channel.
  bool armed{true};
  // Set by a reset that clears the outputs, nothing is sent until the next note starts.
  bool muted{false};
  // True for the one sample on which a note starts.
  bool noteStarted{false};
  int gateRemaining[DANT::CHANS]{};  // samples left of each channel's generated pulse
  int gapRemaining[DANT::CHANS]{};   // samples left of the low period that lets a voice retrigger

  /**
   * Recent input, so that either the signal or the trigger can be delayed.
   */
  float history[POLY_CYCLE_HISTORY]{};
  float trigHistory[POLY_CYCLE_HISTORY]{};
  bool edgeHistory[POLY_CYCLE_HISTORY]{};
  int historyIndex{0};
  int lastTrigDelay{0};

  /**
   * Timing measurement. Every cable adds a sample of delay, so a signal and a trigger that left their source together
   * arrive apart when their routes differ in length. The gap is fixed by the patch, so it is measured on one note and
   * corrected from the next.
   */
  float lastSignal{0.0f};
  int flatSamples{0};
  int stepAge{POLY_CYCLE_LONG_AGO};  // samples since a step that has not yet been matched to a trigger
  int edgeAge{POLY_CYCLE_LONG_AGO};  // samples since a trigger that has not yet been matched to a step
  bool lateStep{false};              // this sample's step belongs to a trigger that has already been acted on

  /**
   * Event matching. A trigger and a jump that belong to the same note can arrive a sample or two apart, as can a
   * reset and the trigger it belongs to. These let the later event recognise the earlier one instead of acting twice.
   */
  float reference{0.0f};  // follows slow movement of the signal, only a jump away from it counts as a change
  int samplesSinceAdvance{POLY_CYCLE_LONG_AGO};
  Source lastSource{SOURCE_NONE};
  int undoChannel{-1};
  float undoValue{0.0f};

  /**
   * Options seen on the previous sample, to notice when they change.
   */
  int lastChannels{0};
  IDLE_MODE lastIdleMode{IDLE_HOLD};
  GATE_MODE lastGateMode{GATE_PASS};

  void reset() { *this = PolyCycle(); }

  /**
   * Call once per sample, the results are in outs and gateOuts.
   * rawSignal, rawTrigVoltage and rawTrigger are the inputs as they arrive, before any timing correction.
   */
  void step(const float rawSignal, const float rawTrigVoltage, const bool rawTrigger, const bool resetTrigger,
            const PolyCycleOpts opts) {
    const int channels{std::min(std::max(opts.channels, 1), DANT::CHANS)};
    const Inputs in{alignInputs(rawSignal, rawTrigVoltage, rawTrigger, opts)};

    applyChannelCount(channels);
    applyIdleMode(opts.idleMode);

    const bool changed{detectChange(in.signal, opts)};

    if (samplesSinceAdvance < POLY_CYCLE_LONG_AGO) {
      ++samplesSinceAdvance;
    }
    const bool recentAdvance{samplesSinceAdvance <= opts.windowSamples};
    noteStarted = false;

    if (resetTrigger) {
      resetChannels(channels, recentAdvance, opts);
    }

    // A trigger and a jump for the same note only advance once, whichever arrives first.
    if (in.trigger) {
      if (!(recentAdvance && lastSource == SOURCE_CHANGE)) {
        advance(SOURCE_TRIGGER, channels, opts.idleMode);
      }
    } else if (changed) {
      if (!(recentAdvance && lastSource == SOURCE_TRIGGER)) {
        advance(SOURCE_CHANGE, channels, opts.idleMode);
      }
    }

    if (noteStarted) {
      muted = false;
    }

    // The outputs are written after any channel change, so a new value that arrives on the same sample as its trigger
    // is only ever seen by the new channel.
    writeSignal(in.signal, opts);
    writeGates(in.trigVoltage, channels, opts);
  }

  /**
   * Timing correction.
   */
  // Stores the raw inputs and reads them back with whichever arrives first delayed to match the other.
  inline Inputs alignInputs(const float rawSignal, const float rawTrigVoltage, const bool rawTrigger,
                            const PolyCycleOpts& opts) {
    int signalDelay{std::min(std::max(opts.delaySamples, 0), POLY_CYCLE_MAX_DELAY)};
    int trigDelay{0};
    lateStep = false;
    if (opts.autoAlign) {
      measureSkew(rawSignal, rawTrigger, std::min(std::max(opts.windowSamples, 1), POLY_CYCLE_MAX_DELAY));
      signalDelay = std::max(-skew, 0);
      trigDelay = std::max(skew, 0);
    } else {
      skew = 0;
      stepAge = POLY_CYCLE_LONG_AGO;
      edgeAge = POLY_CYCLE_LONG_AGO;
    }

    history[historyIndex] = rawSignal;
    trigHistory[historyIndex] = rawTrigVoltage;
    edgeHistory[historyIndex] = rawTrigger;

    Inputs in;
    in.signal = history[tap(signalDelay)];
    in.trigVoltage = trigHistory[tap(trigDelay)];
    in.trigger = readEdge(trigDelay);

    historyIndex = (historyIndex + 1) % POLY_CYCLE_HISTORY;
    return in;
  }

  // Index of the sample that arrived delay samples ago.
  inline int tap(const int delay) { return (historyIndex + POLY_CYCLE_HISTORY - delay) % POLY_CYCLE_HISTORY; }

  // When the trigger delay changes the read position jumps. Moving back would read triggers a second time, moving
  // forward would skip over triggers that have not been acted on yet.
  inline bool readEdge(const int trigDelay) {
    bool edge{false};
    if (trigDelay > lastTrigDelay) {
      for (int d{lastTrigDelay + 1}; d <= trigDelay; ++d) {
        edgeHistory[tap(d)] = false;
      }
    } else if (trigDelay < lastTrigDelay) {
      for (int d{trigDelay + 1}; d <= lastTrigDelay; ++d) {
        edge = edge || edgeHistory[tap(d)];
      }
    }
    lastTrigDelay = trigDelay;
    return edge || edgeHistory[tap(trigDelay)];
  }

  // Matches each trigger to the step in the signal that belongs to it, and records how far apart they arrived.
  inline void measureSkew(const float rawSignal, const bool rawTrigger, const int window) {
    const float movement{std::fabs(rawSignal - lastSignal)};
    const bool stepped{movement > POLY_CYCLE_STEP_MIN && flatSamples >= POLY_CYCLE_FLAT_SAMPLES};
    flatSamples = movement <= POLY_CYCLE_FLAT_EPSILON ? std::min(flatSamples + 1, POLY_CYCLE_LONG_AGO) : 0;
    lastSignal = rawSignal;

    // an event that has waited longer than the window has no partner
    stepAge = stepAge < window ? stepAge + 1 : POLY_CYCLE_LONG_AGO;
    edgeAge = edgeAge < window ? edgeAge + 1 : POLY_CYCLE_LONG_AGO;

    if (rawTrigger && stepped) {
      skew = 0;
      stepAge = POLY_CYCLE_LONG_AGO;
      edgeAge = POLY_CYCLE_LONG_AGO;
    } else if (rawTrigger) {
      if (stepAge < POLY_CYCLE_LONG_AGO) {
        skew = -stepAge;
        stepAge = POLY_CYCLE_LONG_AGO;
      } else {
        edgeAge = 0;
      }
    } else if (stepped) {
      if (edgeAge < POLY_CYCLE_LONG_AGO) {
        lateStep = edgeAge > lastTrigDelay;
        skew = edgeAge;
        edgeAge = POLY_CYCLE_LONG_AGO;
      } else {
        stepAge = 0;
      }
    }
  }

  /**
   * Option changes.
   */
  inline void applyChannelCount(const int channels) {
    if (channels == lastChannels) {
      return;
    }
    std::fill(outs + channels, outs + DANT::CHANS, 0.0f);
    std::fill(gateOuts + channels, gateOuts + DANT::CHANS, 0.0f);
    std::fill(gateRemaining + channels, gateRemaining + DANT::CHANS, 0);
    std::fill(gapRemaining + channels, gapRemaining + DANT::CHANS, 0);
    if (active >= channels) {
      active = 0;
    }
    lastChannels = channels;
  }

  inline void applyIdleMode(const IDLE_MODE idleMode) {
    if (idleMode == lastIdleMode) {
      return;
    }
    if (idleMode == IDLE_ZERO) {
      for (int c{0}; c < DANT::CHANS; ++c) {
        if (c != active) {
          outs[c] = 0.0f;
        }
      }
    }
    lastIdleMode = idleMode;
  }

  /**
   * Channel changes.
   */
  // True when the signal has jumped by the threshold or more. Slow movement is absorbed by the reference instead.
  inline bool detectChange(const float signal, const PolyCycleOpts& opts) {
    if (!opts.advanceOnChange) {
      reference = signal;
      return false;
    }
    if (std::fabs(signal - reference) >= opts.changeThreshold - POLY_CYCLE_CHANGE_TOLERANCE) {
      reference = signal;
      return true;
    }
    reference += (signal - reference) * opts.followCoeff;
    return false;
  }

  inline void advance(const Source source, const int channels, const IDLE_MODE idleMode) {
    if (armed) {
      armed = false;
      undoChannel = -1;
    } else {
      leave(idleMode);
      active = (active + 1) % channels;
      undoChannel = active;
      undoValue = outs[active];
    }
    samplesSinceAdvance = 0;
    lastSource = source;
    noteStarted = true;
  }

  inline void leave(const IDLE_MODE idleMode) {
    if (idleMode == IDLE_ZERO) {
      outs[active] = 0.0f;
    }
  }

  // Returns to the first channel. A note that started just before the reset belongs to it, so that note is moved to
  // the first channel and the channel it had briefly taken is put back as it was.
  inline void resetChannels(const int channels, const bool recentAdvance, const PolyCycleOpts& opts) {
    const float noteValue{outs[active]};
    const int notePulse{gateRemaining[active]};

    if (recentAdvance && undoChannel > 0 && undoChannel < channels) {
      outs[undoChannel] = undoValue;
      gateRemaining[undoChannel] = 0;
    }
    leave(opts.idleMode);
    active = 0;
    armed = !recentAdvance;
    undoChannel = -1;

    if (opts.resetClears) {
      std::fill(outs, outs + DANT::CHANS, 0.0f);
      std::fill(gateRemaining, gateRemaining + DANT::CHANS, 0);
      std::fill(gapRemaining, gapRemaining + DANT::CHANS, 0);
      muted = armed;
    }

    if (recentAdvance) {
      gateRemaining[0] = notePulse;
      if (opts.sampleAndHold) {
        outs[0] = noteValue;
      }
    }
  }

  /**
   * Outputs.
   */
  inline void writeSignal(const float signal, const PolyCycleOpts& opts) {
    if (!opts.sampleAndHold) {
      if (!muted) {
        outs[active] = signal;
      }
      return;
    }
    // A trigger that arrived ahead of its signal has sampled the old value. Until the timing correction has that gap
    // measured it cannot prevent this, so the sample is taken again when the step it was waiting for arrives.
    if (noteStarted || (lateStep && !muted)) {
      outs[active] = signal;
    }
  }

  inline void writeGates(const float trigVoltage, const int channels, const PolyCycleOpts& opts) {
    if (opts.gateMode != lastGateMode) {
      std::fill(gateRemaining, gateRemaining + DANT::CHANS, 0);
      lastGateMode = opts.gateMode;
    }

    // A note starting on a channel whose gate is still high would not be heard as a new note, so the gate is held
    // low first for long enough to be seen as a separate trigger.
    if (noteStarted && gateOuts[active] > POLY_CYCLE_GATE_LOW) {
      gapRemaining[active] = std::max(opts.windowSamples, 1);
    }
    if (noteStarted && opts.gateMode == GATE_PULSE) {
      gateRemaining[active] = std::max(opts.gateSamples, 1);
    }

    for (int c{0}; c < channels; ++c) {
      if (gapRemaining[c] > 0) {
        gateOuts[c] = 0.0f;
        --gapRemaining[c];
      } else if (opts.gateMode == GATE_PULSE && gateRemaining[c] > 0) {
        // Pulses run their full length on the channel they started on, so the voices overlap.
        gateOuts[c] = POLY_CYCLE_GATE_HIGH;
        --gateRemaining[c];
      } else if (opts.gateMode == GATE_PASS && c == active && !muted) {
        // A gate left high on an idle channel would hold its voice open, so only the active channel carries it.
        gateOuts[c] = trigVoltage;
      } else {
        gateOuts[c] = 0.0f;
      }
    }
  }
};

}  // namespace DANT
