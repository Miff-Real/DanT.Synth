#include "../src/dsp/poly-cycle.hpp"

#include "catch2/catch.hpp"

static DANT::PolyCycleOpts make_opts(int channels, DANT::IDLE_MODE idleMode = DANT::IDLE_HOLD, int delaySamples = 0) {
  DANT::PolyCycleOpts o;
  o.channels = channels;
  o.idleMode = idleMode;
  o.delaySamples = delaySamples;
  o.windowSamples = 48;
  return o;
}

static DANT::PolyCycleOpts make_change_opts(int channels, float changeThreshold = 1.0f / 12.0f) {
  DANT::PolyCycleOpts o{make_opts(channels)};
  o.advanceOnChange = true;
  o.changeThreshold = changeThreshold;
  o.followCoeff = 0.02f;
  return o;
}

static const float GATE_HIGH{10.0f};
static const float GATE_LOW{0.0f};
static const float SEMITONE{1.0f / 12.0f};

// the trigger input is a short pulse in these cases, high only on the sample it fires
static void step(DANT::PolyCycle& cycle, float signal, bool trigger, bool reset, const DANT::PolyCycleOpts& opts) {
  cycle.step(signal, trigger ? GATE_HIGH : GATE_LOW, trigger, reset, opts);
}

static void settle(DANT::PolyCycle& cycle, float signal, float gate, const DANT::PolyCycleOpts& opts,
                   int samples = 100) {
  for (int i{0}; i < samples; ++i) {
    cycle.step(signal, gate, false, false, opts);
  }
}

static const bool TRIG{true};
static const bool NO_TRIG{false};
static const bool RESET{true};
static const bool NO_RESET{false};

TEST_CASE("poly-cycle.hpp::PolyCycle") {
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

  SECTION("two triggers close together both advance") {
    const DANT::PolyCycleOpts opts{make_opts(4)};
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.active == 2);
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

  SECTION("a channel reused before its pulse ends drops low for one sample") {
    DANT::PolyCycleOpts opts{make_opts(1)};
    opts.gateMode = DANT::GATE_PULSE;
    opts.gateSamples = 10;
    step(cycle, 1.0f, TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 1.0f, NO_TRIG, NO_RESET, opts);
    CHECK(cycle.gateOuts[0] == GATE_HIGH);
    step(cycle, 2.0f, TRIG, NO_RESET, opts);  // next note, same channel
    CHECK(cycle.gateOuts[0] == GATE_LOW);
    step(cycle, 2.0f, NO_TRIG, NO_RESET, opts);
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
}
