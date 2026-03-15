#include "../src/dsp/bend-voct.hpp"

#include <rack.hpp>
#include <string>
#include <vector>

#include "catch2/catch.hpp"

const float FP_TOLERANCE_VOCT = 1e-6f;

static void check_voct_approx_equal(const rack::simd::float_4& input, const rack::simd::float_4& actual,
                                    const rack::simd::float_4& expected) {
  for (int i{0}; i < 4; ++i) {
    Catch::Detail::Approx target = Catch::Detail::Approx(expected[i]).epsilon(FP_TOLERANCE_VOCT);

    UNSCOPED_INFO("input [" << input[i] << "]");
    CHECK(actual[i] == target);
  }
}

struct BendTestCaseParams {
  std::string name;
  DANT::BendOpts opts;
  rack::simd::float_4 inSignals;
  rack::simd::float_4 expectedSignals;
};

TEST_CASE("bend-voct.hpp::bendVoct") {
  auto make_opts = [](float start, float target, rack::simd::float_4 p, float shape) {
    DANT::BendOpts o;
    o.startOffsets = rack::simd::float_4(start);
    o.targetOffsets = rack::simd::float_4(target);
    o.progress = p;
    o.shape = rack::simd::float_4(shape);
    return o;
  };

  std::vector<BendTestCaseParams> testSuite = {
      {"Linear curve (shape = 0)", make_opts(0.0f, 1.0f, rack::simd::float_4(0.0f, 0.25f, 0.5f, 1.0f), 0.0f),
       rack::simd::float_4(0.0f, 1.0f, 2.0f, 3.0f), rack::simd::float_4(0.0f, 1.25f, 2.5f, 4.0f)},
      {"Exponential curve (shape = 1)", make_opts(0.0f, 1.0f, rack::simd::float_4(0.0f, 0.25f, 0.5f, 1.0f), 1.0f),
       rack::simd::float_4(0.0f, 1.0f, 2.0f, 3.0f), rack::simd::float_4(0.0f, 1.00390625f, 2.0625f, 4.0f)},
      {"Logarithmic curve (shape = -1)", make_opts(0.0f, 1.0f, rack::simd::float_4(0.0f, 0.25f, 0.5f, 1.0f), -1.0f),
       rack::simd::float_4(0.0f, 1.0f, 2.0f, 3.0f), rack::simd::float_4(0.0f, 1.70710678f, 2.84089642f, 4.0f)},
      {"Clamped progress limits to [0, 1]", make_opts(0.0f, 2.0f, rack::simd::float_4(-1.0f, 0.0f, 1.0f, 2.0f), 0.0f),
       rack::simd::float_4(0.0f), rack::simd::float_4(0.0f, 0.0f, 2.0f, 2.0f)},
      {"Bending down (start > target)", make_opts(1.0f, 0.0f, rack::simd::float_4(0.0f, 0.25f, 0.5f, 1.0f), 0.0f),
       rack::simd::float_4(0.0f, 1.0f, 2.0f, 3.0f), rack::simd::float_4(1.0f, 1.75f, 2.5f, 3.0f)},
      {"Bending with both non-zero start and target",
       make_opts(-1.0f, 1.0f, rack::simd::float_4(0.0f, 0.25f, 0.5f, 1.0f), 0.0f),
       rack::simd::float_4(0.0f, 1.0f, 2.0f, 3.0f), rack::simd::float_4(-1.0f, 0.5f, 2.0f, 4.0f)}};

  for (const auto& testCase : testSuite) {
    SECTION(testCase.name) {
      rack::simd::float_4 outSignals = DANT::bendVoct(testCase.inSignals, testCase.opts);
      check_voct_approx_equal(testCase.inSignals, outSignals, testCase.expectedSignals);
    }
  }
}
