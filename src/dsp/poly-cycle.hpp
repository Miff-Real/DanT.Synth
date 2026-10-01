#pragma once

#include <algorithm>  // std::fill std::min std::max
#include <cmath>      // std::fabs

#include "../static.hpp"

namespace DANT {

static const int POLY_CYCLE_MAX_DELAY{8};  // samples
static const int POLY_CYCLE_LONG_AGO{1 << 20};
// Adjacent notes are a semitone apart only to within floating point error, without this a threshold of exactly one
// semitone would miss some of them.
static const float POLY_CYCLE_CHANGE_TOLERANCE{0.001f};
static const float POLY_CYCLE_GATE_HIGH{10.0f};

enum IDLE_MODE { IDLE_HOLD, IDLE_ZERO };
enum GATE_MODE { GATE_PASS, GATE_OFF, GATE_PULSE };

struct PolyCycleOpts {
  int channels{1};                      // number of output channels to cycle through
  IDLE_MODE idleMode{IDLE_HOLD};        // what the channels that are not receiving the signal output
  int delaySamples{0};                  // delays the signal, for patches where it arrives before its trigger
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
 */
struct PolyCycle {
  enum Source { SOURCE_NONE, SOURCE_TRIGGER, SOURCE_CHANGE };

  float outs[DANT::CHANS]{};
  float gateOuts[DANT::CHANS]{};
  int gateRemaining[DANT::CHANS]{};  // samples left of each channel's generated pulse
  int active{0};
  // The next advance keeps the active channel, so the first note after start up or a reset lands on the first
  // channel.
  bool armed{true};

  float history[POLY_CYCLE_MAX_DELAY + 1]{};
  int historyIndex{0};

  // Follows slow movement of the signal, so that only a jump away from it counts as a change.
  float reference{0.0f};

  // A trigger and a jump that belong to the same note can arrive a sample or two apart, as can a reset and the
  // trigger it belongs to. These let the later event recognise the earlier one instead of acting twice.
  int samplesSinceAdvance{POLY_CYCLE_LONG_AGO};
  Source lastSource{SOURCE_NONE};
  int undoChannel{-1};
  float undoValue{0.0f};

  bool noteStarted{false};

  int lastChannels{0};
  IDLE_MODE lastIdleMode{IDLE_HOLD};
  GATE_MODE lastGateMode{GATE_PASS};

  void reset() {
    std::fill(outs, outs + DANT::CHANS, 0.0f);
    std::fill(gateOuts, gateOuts + DANT::CHANS, 0.0f);
    std::fill(gateRemaining, gateRemaining + DANT::CHANS, 0);
    std::fill(history, history + POLY_CYCLE_MAX_DELAY + 1, 0.0f);
    historyIndex = 0;
    active = 0;
    armed = true;
    reference = 0.0f;
    samplesSinceAdvance = POLY_CYCLE_LONG_AGO;
    lastSource = SOURCE_NONE;
    undoChannel = -1;
    undoValue = 0.0f;
    noteStarted = false;
    lastChannels = 0;
    lastIdleMode = IDLE_HOLD;
    lastGateMode = GATE_PASS;
  }

  /**
   * Call once per sample, the results are in outs and gateOuts.
   */
  void step(const float signal, const float trigVoltage, const bool trigger, const bool resetTrigger,
            const PolyCycleOpts opts) {
    const int channels{std::min(std::max(opts.channels, 1), DANT::CHANS)};
    const int delay{std::min(std::max(opts.delaySamples, 0), POLY_CYCLE_MAX_DELAY)};

    history[historyIndex] = signal;
    const float delayed{history[(historyIndex + (POLY_CYCLE_MAX_DELAY + 1) - delay) % (POLY_CYCLE_MAX_DELAY + 1)]};
    historyIndex = (historyIndex + 1) % (POLY_CYCLE_MAX_DELAY + 1);

    if (channels != lastChannels) {
      std::fill(outs + channels, outs + DANT::CHANS, 0.0f);
      std::fill(gateOuts + channels, gateOuts + DANT::CHANS, 0.0f);
      std::fill(gateRemaining + channels, gateRemaining + DANT::CHANS, 0);
      if (active >= channels) {
        active = 0;
      }
      lastChannels = channels;
    }

    if (samplesSinceAdvance < POLY_CYCLE_LONG_AGO) {
      ++samplesSinceAdvance;
    }

    bool changed{false};
    if (opts.advanceOnChange && std::fabs(delayed - reference) >= opts.changeThreshold - POLY_CYCLE_CHANGE_TOLERANCE) {
      changed = true;
      reference = delayed;
    } else if (opts.advanceOnChange) {
      reference += (delayed - reference) * opts.followCoeff;
    } else {
      reference = delayed;
    }

    const bool recentAdvance{samplesSinceAdvance <= opts.windowSamples};
    noteStarted = false;

    if (resetTrigger) {
      if (recentAdvance && undoChannel > 0 && undoChannel < channels) {
        outs[undoChannel] = undoValue;
        gateRemaining[0] = gateRemaining[undoChannel];
        gateRemaining[undoChannel] = 0;
      }
      leave(opts.idleMode);
      active = 0;
      armed = !recentAdvance;
      undoChannel = -1;
    }

    if (trigger) {
      if (!(recentAdvance && lastSource == SOURCE_CHANGE)) {
        advance(SOURCE_TRIGGER, channels, opts.idleMode);
      }
    } else if (changed) {
      if (!(recentAdvance && lastSource == SOURCE_TRIGGER)) {
        advance(SOURCE_CHANGE, channels, opts.idleMode);
      }
    }

    if (opts.idleMode != lastIdleMode) {
      if (opts.idleMode == IDLE_ZERO) {
        for (int c{0}; c < DANT::CHANS; ++c) {
          if (c != active) {
            outs[c] = 0.0f;
          }
        }
      }
      lastIdleMode = opts.idleMode;
    }

    // The outputs are written after any channel change, so a new value that arrives on the same sample as its trigger
    // is only ever seen by the new channel.
    outs[active] = delayed;
    writeGates(trigVoltage, channels, opts);
  }

  inline void writeGates(const float trigVoltage, const int channels, const PolyCycleOpts opts) {
    if (opts.gateMode != lastGateMode) {
      std::fill(gateRemaining, gateRemaining + DANT::CHANS, 0);
      lastGateMode = opts.gateMode;
    }

    if (opts.gateMode != GATE_PULSE) {
      // A gate left high on an idle channel would hold its voice open, so only the active channel carries it.
      std::fill(gateOuts, gateOuts + channels, 0.0f);
      if (opts.gateMode == GATE_PASS) {
        gateOuts[active] = trigVoltage;
      }
      return;
    }

    // Pulses run their full length on the channel they started on, so the voices overlap.
    bool stillHigh{false};
    if (noteStarted) {
      stillHigh = gateRemaining[active] > 0;
      gateRemaining[active] = std::max(opts.gateSamples, 1);
    }
    for (int c{0}; c < channels; ++c) {
      if (gateRemaining[c] > 0) {
        gateOuts[c] = POLY_CYCLE_GATE_HIGH;
        --gateRemaining[c];
      } else {
        gateOuts[c] = 0.0f;
      }
    }
    if (stillHigh) {
      // The channel has come round again before its last pulse finished, one low sample gives the voice a new edge.
      gateOuts[active] = 0.0f;
    }
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
};

}  // namespace DANT
