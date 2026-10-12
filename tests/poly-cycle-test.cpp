#include "../src/dsp/poly-cycle.hpp"

#include "catch2/catch.hpp"

static const float GATE_HIGH{10.0f};
static const float GATE_LOW{0.0f};
static const float SEMITONE{1.0f / 12.0f};

// names for the trigger and reset arguments, so that a call reads as what happened on that sample
static const bool TRIG{true};
static const bool NO_TRIG{false};
static const bool RESET{true};
static const bool NO_RESET{false};

static DANT::PolyCycleOpts make_opts(int channels, DANT::IDLE_MODE idleMode = DANT::IDLE_HOLD, int delaySamples = 0) {
  DANT::PolyCycleOpts o;
  o.channels = channels;
  o.idleMode = idleMode;
  o.delaySamples = delaySamples;
  o.windowSamples = 48;
  return o;
}

// options with automatic channel increment turned on
static DANT::PolyCycleOpts make_change_opts(int channels, float changeThreshold = SEMITONE) {
  DANT::PolyCycleOpts o{make_opts(channels)};
  o.advanceOnChange = true;
  o.changeThreshold = changeThreshold;
  o.followCoeff = 0.02f;
  return o;
}

// one sample where the trigger input is a short pulse, high only on the sample it fires
static void step(DANT::PolyCycle& cycle, float signal, bool trigger, bool reset, const DANT::PolyCycleOpts& opts) {
  cycle.step(signal, trigger ? GATE_HIGH : GATE_LOW, trigger, reset, opts);
}

// a run of samples with nothing happening, long enough for one note to be finished before the next
static void settle(DANT::PolyCycle& cycle, float signal, float gate, const DANT::PolyCycleOpts& opts,
                   int samples = 100) {
  for (int i{0}; i < samples; ++i) {
    cycle.step(signal, gate, NO_TRIG, NO_RESET, opts);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle routing") {
  DANT::PolyCycle cycle;

  SECTION("signal passes to the first channel before any trigger") {
    step(cycle, 1.0f, NO_TRIG, NO_RESET, make_opts(4));
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 0.0f);
  }

  SECTION("first trigger keeps the first channel, later triggers advance and wrap") {
    const DANT::PolyCycleOpts opts{make_opts(3)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    step(cycle, 3.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 2);
    step(cycle, 4.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 4.0f);
    CHECK(cycle.outs[1] == 2.0f);
    CHECK(cycle.outs[2] == 3.0f);
  }

  SECTION("a value arriving with its trigger never reaches the previous channel") {
    const DANT::PolyCycleOpts opts{make_opts(2)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);  // new note and its trigger on the same sample
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("idle channels hold their last value in hold mode") {
    const DANT::PolyCycleOpts opts{make_opts(2, DANT::IDLE_HOLD)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.5f, NO_TRIG, NO_RESET, opts);  // the active channel keeps tracking
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.5f);
  }

  SECTION("idle channels output zero in zero mode") {
    const DANT::PolyCycleOpts opts{make_opts(2, DANT::IDLE_ZERO)};
    step(cycle, 10.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 10.0f);
    step(cycle, 10.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 0.0f);
    CHECK(cycle.outs[1] == 10.0f);
  }

  SECTION("switching to zero mode clears the held channels") {
    step(cycle, 1.0f, TRIG, NO_RESET, make_opts(2, DANT::IDLE_HOLD));
    step(cycle, 2.0f, TRIG, NO_RESET, make_opts(2, DANT::IDLE_HOLD));
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, make_opts(2, DANT::IDLE_ZERO));
    CHECK(cycle.outs[0] == 0.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("reducing the channel count brings the active channel back in range") {
    step(cycle, 1.0f, TRIG, NO_RESET, make_opts(4));
    step(cycle, 2.0f, TRIG, NO_RESET, make_opts(4));
    step(cycle, 3.0f, TRIG, NO_RESET, make_opts(4));
    CHECK(cycle.active == 2);
    step(cycle, 4.0f, NO_TRIG, NO_RESET, make_opts(2));
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 4.0f);
    CHECK(cycle.outs[2] == 0.0f);
  }

  SECTION("channel count is limited to the supported range") {
    step(cycle, 1.0f, TRIG, NO_RESET, make_opts(0));
    step(cycle, 2.0f, TRIG, NO_RESET, make_opts(0));
    CHECK(cycle.active == 0);
    const DANT::PolyCycleOpts opts{make_opts(99)};
    for (int i{0}; i < DANT::CHANS; ++i) {
      step(cycle, 1.0f, TRIG, NO_RESET, opts);
    }
    CHECK(cycle.active == 0);
  }

  SECTION("two triggers close together both advance") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 2);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle signal delay") {
  DANT::PolyCycle cycle;

  SECTION("signal arriving before its trigger overwrites the previous channel without a delay") {
    const DANT::PolyCycleOpts opts{make_opts(2)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);  // new note, two samples ahead of its trigger
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 2.0f);  // the old voice has been left holding the new note
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("signal delay protects the previous channel when the signal arrives before its trigger") {
    const DANT::PolyCycleOpts opts{make_opts(2, DANT::IDLE_HOLD, 2)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);  // new note, two samples ahead of its trigger
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("signal delay is limited to the buffer length") {
    const DANT::PolyCycleOpts opts{make_opts(1, DANT::IDLE_HOLD, 100)};
    for (int i{0}; i < DANT::POLY_CYCLE_MAX_DELAY; ++i) {
      step(cycle, 5.0f, NO_TRIG, NO_RESET, opts);
      CHECK(cycle.outs[0] == 0.0f);
    }
    step(cycle, 5.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 5.0f);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle reset") {
  DANT::PolyCycle cycle;

  SECTION("reset returns to the first channel and the next trigger stays there") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    step(cycle, 3.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 2);
    for (int i{0}; i < 100; ++i) {
      step(cycle, 3.0f, NO_TRIG, NO_RESET, opts);
    }
    step(cycle, 4.0f, NO_TRIG, RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 4.0f);
    CHECK(cycle.outs[2] == 3.0f);
    step(cycle, 5.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    step(cycle, 6.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
  }

  SECTION("reset and trigger on the same sample start on the first channel") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    for (int i{0}; i < 100; ++i) {
      step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    }
    step(cycle, 3.0f, TRIG, RESET, opts);
    CHECK(cycle.active == 0);
    step(cycle, 4.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
  }

  SECTION("reset arriving just after its trigger undoes that trigger") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    step(cycle, 3.0f, TRIG, NO_RESET, opts);  // channels now hold 1, 2, 3, 0
    for (int i{0}; i < 100; ++i) {
      step(cycle, 3.0f, NO_TRIG, NO_RESET, opts);
    }
    step(cycle, 9.0f, TRIG, NO_RESET, opts);  // first note of the next loop lands on the fourth channel
    CHECK(cycle.active == 3);
    CHECK(cycle.outs[3] == 9.0f);
    step(cycle, 9.0f, NO_TRIG, RESET, opts);  // its reset arrives one sample late
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 9.0f);
    CHECK(cycle.outs[3] == 0.0f);             // put back as it was
    step(cycle, 8.0f, TRIG, NO_RESET, opts);  // the following note moves on, it does not reuse the first channel
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 9.0f);
    CHECK(cycle.outs[1] == 8.0f);
  }

  SECTION("reset clears: every channel of both outputs is zero until the next note") {
    DANT::PolyCycleOpts opts{make_opts(3)};
    opts.resetClears = true;
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 2.0f, GATE_HIGH, opts);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);

    cycle.step(2.0f, GATE_HIGH, NO_TRIG, RESET, opts);  // the gate is still held when the reset arrives
    CHECK(cycle.active == 0);
    for (int c{0}; c < 3; ++c) {
      CHECK(cycle.outs[c] == 0.0f);
      CHECK(cycle.gateOuts[c] == GATE_LOW);
    }
    settle(cycle, 2.0f, GATE_HIGH, opts);  // the input is still there, nothing is sent
    CHECK(cycle.outs[0] == 0.0f);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    settle(cycle, 2.0f, GATE_LOW, opts);

    cycle.step(3.0f, GATE_HIGH, TRIG, NO_RESET, opts);  // the next note plays on the first channel
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 3.0f);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    CHECK(cycle.outs[1] == 0.0f);
  }

  SECTION("reset clears: a reset arriving with or just after its trigger keeps that note") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.resetClears = true;
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 2.0f, GATE_LOW, opts);

    cycle.step(3.0f, GATE_HIGH, TRIG, RESET, opts);  // together
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 3.0f);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    CHECK(cycle.outs[1] == 0.0f);
    settle(cycle, 3.0f, GATE_LOW, opts);
    cycle.step(4.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    settle(cycle, 4.0f, GATE_LOW, opts);

    cycle.step(5.0f, GATE_HIGH, TRIG, NO_RESET, opts);  // trigger first
    CHECK(cycle.active == 2);
    cycle.step(5.0f, GATE_HIGH, NO_TRIG, RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 5.0f);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    CHECK(cycle.outs[1] == 0.0f);
    CHECK(cycle.outs[2] == 0.0f);
    settle(cycle, 5.0f, GATE_LOW, opts);
    cycle.step(6.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
  }

  SECTION("without reset clears a reset leaves the held values alone") {
    const DANT::PolyCycleOpts opts{make_opts(3)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    settle(cycle, 2.0f, GATE_LOW, opts);
    step(cycle, 2.0f, NO_TRIG, RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 2.0f);  // the first channel follows the input straight away
    CHECK(cycle.outs[1] == 2.0f);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle trigger output") {
  DANT::PolyCycle cycle;

  SECTION("trigger voltage follows the signal onto the active channel") {
    const DANT::PolyCycleOpts opts{make_opts(3)};
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);  // gate held
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    cycle.step(1.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);  // gate released
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
    CHECK(cycle.gateOuts[2] == GATE_LOW);
  }

  SECTION("trigger output of idle channels is zero even when the signal is held") {
    const DANT::PolyCycleOpts opts{make_change_opts(2)};
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_HIGH, opts);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);  // legato, the gate never falls
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
  }

  SECTION("trigger output is silent when turned off") {
    DANT::PolyCycleOpts opts{make_opts(2)};
    opts.gateMode = DANT::GATE_OFF;
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    CHECK(cycle.gateOuts[1] == GATE_LOW);
  }

  SECTION("a pulse is made for each note, on the channel the note lands on") {
    DANT::PolyCycleOpts opts{make_change_opts(3)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 3;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);  // first note, a jump from the resting 0V
    CHECK(cycle.active == 0);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);  // three samples long
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
  }

  SECTION("pulses on different channels overlap") {
    DANT::PolyCycleOpts opts{make_opts(3)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 10;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);  // still running after the signal has moved on
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
    CHECK(cycle.gateOuts[2] == GATE_LOW);
  }

  SECTION("a channel reused before its pulse ends drops low long enough to be retriggered") {
    DANT::PolyCycleOpts opts{make_opts(1)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 100;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);  // next note, same channel
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    for (int i{1}; i < opts.windowSamples; ++i) {
      step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
      CHECK(cycle.gateOuts[0] == GATE_LOW);
    }
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    settle(cycle, 2.0f, GATE_LOW, opts, 98);  // the pulse still runs its full length after the low period
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
  }

  SECTION("a passed gate that is still high when a note starts drops low long enough to be retriggered") {
    const DANT::PolyCycleOpts opts{make_change_opts(1)};
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_HIGH, opts);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);  // legato note on the only channel
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    settle(cycle, 2.0f, GATE_HIGH, opts, opts.windowSamples - 1);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
  }

  SECTION("a reset just after its trigger moves the pulse to the first channel") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 10;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
    step(cycle, 2.0f, NO_TRIG, RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    CHECK(cycle.gateOuts[1] == GATE_LOW);
  }

  SECTION("changing the trigger output mode stops any running pulses") {
    DANT::PolyCycleOpts opts{make_opts(2)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 10;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    opts.gateMode = DANT::GATE_OFF;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    opts.gateMode = DANT::GATE_PULSE;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
  }

  SECTION("reset clears: running pulses are stopped") {
    DANT::PolyCycleOpts opts{make_opts(3)};
    opts.resetClears = true;
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 1000;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    settle(cycle, 2.0f, GATE_LOW, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
    step(cycle, 2.0f, NO_TRIG, RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    CHECK(cycle.gateOuts[1] == GATE_LOW);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle automatic channel increment") {
  DANT::PolyCycle cycle;

  SECTION("a jump of the threshold or more advances when change detection is on") {
    const DANT::PolyCycleOpts opts{make_change_opts(4)};
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);  // first note takes the first channel
    CHECK(cycle.active == 0);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 1.0f + SEMITONE, NO_TRIG, NO_RESET, opts);  // exactly one semitone
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 1.0f + SEMITONE);
    settle(cycle, 1.0f + SEMITONE, GATE_LOW, opts);
    step(cycle, 1.0f + SEMITONE - 0.5f, NO_TRIG, NO_RESET, opts);  // downward jumps count too
    CHECK(cycle.active == 2);
  }

  SECTION("a jump smaller than the threshold does not advance") {
    const DANT::PolyCycleOpts opts{make_change_opts(4)};
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 1.0f + (SEMITONE * 0.5f), NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 1.0f + (SEMITONE * 0.5f));
  }

  SECTION("slow movement does not advance however far it goes") {
    const DANT::PolyCycleOpts opts{make_change_opts(4)};
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    float glide{1.0f};
    for (int i{0}; i < 4800; ++i) {  // half an octave over a tenth of a second at 48kHz
      glide += 0.5f / 4800.0f;
      step(cycle, glide, NO_TRIG, NO_RESET, opts);
    }
    CHECK(cycle.active == 0);
  }

  SECTION("jumps are ignored when change detection is off") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 5.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 5.0f);
  }

  SECTION("a jump and a trigger for the same note advance once, whichever arrives first") {
    const DANT::PolyCycleOpts opts{make_change_opts(4)};
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);

    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);  // together
    CHECK(cycle.active == 1);
    settle(cycle, 2.0f, GATE_LOW, opts);

    cycle.step(3.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);  // jump first
    cycle.step(3.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 2);
    CHECK(cycle.outs[1] == 2.0f);  // the previous voice never saw the new note
    CHECK(cycle.gateOuts[2] == GATE_HIGH);
    settle(cycle, 3.0f, GATE_LOW, opts);

    cycle.step(3.0f, GATE_HIGH, TRIG, NO_RESET, opts);  // trigger first
    cycle.step(4.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 3);
    CHECK(cycle.outs[2] == 3.0f);
    CHECK(cycle.outs[3] == 4.0f);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle timing correction") {
  DANT::PolyCycle cycle;

  SECTION("nothing is delayed when the signal and trigger arrive together") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 0);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    settle(cycle, 1.0f, GATE_LOW, opts);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 0);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
  }

  SECTION("a trigger that arrives before its signal is measured, then held back to match") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);

    // first note: trigger two samples ahead of the new value, this is the note the gap is measured on
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    cycle.step(0.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 2);
    CHECK(cycle.active == 0);  // the trigger is not acted on a second time when the correction starts
    settle(cycle, 1.0f, GATE_HIGH, opts, 20);
    CHECK(cycle.active == 0);
    settle(cycle, 1.0f, GATE_LOW, opts);

    // second note: the new channel must never output the old value, and its gate must rise with the new value
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.gateOuts[1] == GATE_LOW);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 0);
    CHECK(cycle.gateOuts[1] == GATE_LOW);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
    CHECK(cycle.skew == 2);
  }

  SECTION("a signal that arrives before its trigger is measured, then held back to match") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);

    // first note: new value three samples ahead of its trigger
    cycle.step(1.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.skew == -3);
    CHECK(cycle.outs[0] == 1.0f);
    settle(cycle, 1.0f, GATE_LOW, opts);

    // second note: the previous channel must never output the new value
    cycle.step(2.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    cycle.step(2.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    cycle.step(2.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    cycle.step(2.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    CHECK(cycle.gateOuts[1] == GATE_HIGH);
  }

  SECTION("a repeated note with no step keeps the last measurement") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 1);
    settle(cycle, 1.0f, GATE_LOW, opts);
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);  // same value again
    settle(cycle, 1.0f, GATE_LOW, opts);
    CHECK(cycle.skew == 1);
    CHECK(cycle.active == 1);
  }

  SECTION("a signal that is always moving is never measured") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    float ramp{0.0f};
    for (int i{0}; i < 400; ++i) {
      ramp += 0.01f;
      const bool trig{i % 100 == 50};
      cycle.step(ramp, trig ? GATE_HIGH : GATE_LOW, trig, NO_RESET, opts);
    }
    CHECK(cycle.skew == 0);
    CHECK(cycle.active == 3);
  }

  SECTION("events further apart than the window are not matched") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 0.0f, GATE_LOW, opts, opts.windowSamples + 5);
    cycle.step(1.0f, GATE_LOW, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 0);
  }

  SECTION("nothing is measured when the correction is turned off") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    settle(cycle, 0.0f, GATE_LOW, opts, 10);
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    cycle.step(0.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 0);
  }

  SECTION("a change in the patch is picked up on the next note") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    settle(cycle, 0.0f, GATE_HIGH, opts, 4);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 5);
    settle(cycle, 1.0f, GATE_LOW, opts);
    const int before{cycle.active};
    // the trigger now arrives only one sample ahead, it must still be acted on exactly once
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.skew == 1);
    settle(cycle, 2.0f, GATE_LOW, opts);
    CHECK(cycle.active == before + 1);
  }
}

TEST_CASE("poly-cycle.hpp::PolyCycle sample and hold") {
  DANT::PolyCycle cycle;

  SECTION("the value is taken when a note starts and then held") {
    DANT::PolyCycleOpts opts{make_opts(3)};
    opts.sampleAndHold = true;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);  // nothing sampled before the first note
    CHECK(cycle.outs[0] == 0.0f);
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 1.5f, NO_TRIG, NO_RESET, opts);  // the input moves on, the channel does not follow
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
    step(cycle, 2.5f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("a channel falls to zero when the signal moves on, in zero mode") {
    DANT::PolyCycleOpts opts{make_opts(3, DANT::IDLE_ZERO)};
    opts.sampleAndHold = true;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.5f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 0.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("the sample is taken from the delayed signal") {
    DANT::PolyCycleOpts opts{make_opts(2, DANT::IDLE_HOLD, 2)};
    opts.sampleAndHold = true;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 3.0f, TRIG, NO_RESET, opts);  // the value from two samples ago is what has reached the output
    CHECK(cycle.outs[0] == 1.0f);
  }

  SECTION("a jump samples the new value when advancing on change") {
    DANT::PolyCycleOpts opts{make_change_opts(3)};
    opts.sampleAndHold = true;
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("a trigger ahead of its signal still ends up holding the new value") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.sampleAndHold = true;
    opts.autoAlign = true;
    settle(cycle, 0.0f, GATE_LOW, opts, 10);

    // first note, before the gap has been measured: the old value is sampled, then replaced when the step arrives
    cycle.step(0.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 0.0f);
    cycle.step(0.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[0] == 1.0f);
    cycle.step(1.3f, GATE_HIGH, NO_TRIG, NO_RESET, opts);  // later movement is not followed
    CHECK(cycle.outs[0] == 1.0f);
    settle(cycle, 1.0f, GATE_LOW, opts);
    CHECK(cycle.outs[0] == 1.0f);

    // second note, now corrected: the new channel goes straight to the new value
    cycle.step(1.0f, GATE_HIGH, TRIG, NO_RESET, opts);
    cycle.step(1.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.outs[1] == 0.0f);
    cycle.step(2.0f, GATE_HIGH, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.active == 1);
    CHECK(cycle.outs[0] == 1.0f);
    CHECK(cycle.outs[1] == 2.0f);
  }

  SECTION("reset clears with sample and hold: the note that came with the reset keeps its sample") {
    DANT::PolyCycleOpts opts{make_opts(4)};
    opts.resetClears = true;
    opts.sampleAndHold = true;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    settle(cycle, 1.0f, GATE_LOW, opts);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.outs[1] == 2.0f);
    step(cycle, 2.4f, NO_TRIG, RESET, opts);  // reset one sample late, the input has already moved
    CHECK(cycle.active == 0);
    CHECK(cycle.outs[0] == 2.0f);
    CHECK(cycle.outs[1] == 0.0f);
  }
}
