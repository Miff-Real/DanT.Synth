#include "../src/dsp/att-off-clip-rect.hpp"

#include <rack.hpp>

#include "catch2/catch.hpp"

const float FP_TOLERANCE = 1e-6f;

void require_float4_approx_equal(const rack::simd::float_4& input, const rack::simd::float_4& actual,
                                 const rack::simd::float_4& expected) {
  for (int i{0}; i < 4; ++i) {
    Catch::Detail::Approx target = Catch::Detail::Approx(expected[i]).epsilon(FP_TOLERANCE);

    UNSCOPED_INFO("input [" << input[i] << "]");
    CHECK(actual[i] == target);
  }
}

TEST_CASE("att-off-clip-rect.hpp::attenuvertOffsetClipRectify") {
  SECTION("Default options") {
    DANT::AtOCROpts opts;  // Default options: 1.0f attenuversion, 0.0f offset, NO_CLIP, NO_RECT
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.5f);
    rack::simd::float_4 expected = rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.attenuversion = 0.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 0.5f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(0.5f, -1.0f, 1.5f, -2.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.attenuversion = 2.0f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 2.0f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(2.0f, -4.0f, 6.0f, -8.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.attenuversion = -1.0f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -1.0f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(-1.0f, 2.0f, -3.0f, 4.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.attenuversion = 0.0f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 0.0f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(0.0f, 0.0f, 0.0f, 0.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.offset = 1.0f") {
    DANT::AtOCROpts opts;
    opts.offset = 1.0f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(2.0f, -1.0f, 4.0f, -3.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.offset = -0.5f") {
    DANT::AtOCROpts opts;
    opts.offset = -0.5f;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, -2.0f, 3.0f, -4.0f);
    rack::simd::float_4 expected = rack::simd::float_4(0.5f, -2.5f, 2.5f, -4.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = AT_THEN_OFF, opts.attenuversion = 2.0f, opts.offset = 1.0f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 2.0f;
    opts.offset = 1.0f;
    opts.opOrder = DANT::AT_THEN_OFF;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(3.0f, 6.0f, -1.0f, -4.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = AT_THEN_OFF, opts.attenuversion = -2.0f, opts.offset = -1.0f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -2.0f;
    opts.offset = -1.0f;
    opts.opOrder = DANT::AT_THEN_OFF;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(-3.0f, -6.0f, 1.0f, 4.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT") {
    DANT::AtOCROpts opts;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = 2.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 2.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(2.5f, 6.25f, -2.5f, -6.25f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = -2.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -2.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(-2.5f, -6.25f, 2.5f, 6.25f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = 2.0f, opts.offset = 1.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 2.0f;
    opts.offset = 1.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(5.0f, 8.0f, 1.0f, -2.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = 2.0f, opts.offset = -1.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 2.0f;
    opts.offset = -1.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(-1.0f, 2.0f, -5.0f, -8.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = -2.0f, opts.offset = 1.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -2.0f;
    opts.offset = 1.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(-5.0f, -8.0f, -1.0f, 2.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.opOrder = OFF_THEN_AT, opts.attenuversion = -2.0f, opts.offset = -1.5f") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -2.0f;
    opts.offset = -1.5f;
    opts.opOrder = DANT::OFF_THEN_AT;
    rack::simd::float_4 inSignals = rack::simd::float_4(1.0f, 2.5f, -1.0f, -2.5f);
    rack::simd::float_4 expected = rack::simd::float_4(1.0f, -2.0f, 5.0f, 8.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.clipLvl = NO_CLIP") {
    DANT::AtOCROpts opts;
    opts.clipLvl = DANT::NO_CLIP;
    rack::simd::float_4 inSignals = rack::simd::float_4(5.5f, 11.5f, -6.5f, -12.5f);
    rack::simd::float_4 expected = rack::simd::float_4(5.5f, 11.5f, -6.5f, -12.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.clipLvl = TEN_CLIP") {
    DANT::AtOCROpts opts;
    opts.clipLvl = DANT::TEN_CLIP;
    rack::simd::float_4 inSignals = rack::simd::float_4(5.5f, 11.5f, -6.5f, -12.5f);
    rack::simd::float_4 expected = rack::simd::float_4(5.5f, 10.0f, -6.5f, -10.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.clipLvl = FIVE_CLIP") {
    DANT::AtOCROpts opts;
    opts.clipLvl = DANT::FIVE_CLIP;
    rack::simd::float_4 inSignals = rack::simd::float_4(5.5f, 11.5f, -6.5f, -12.5f);
    rack::simd::float_4 expected = rack::simd::float_4(5.0f, 5.0f, -5.0f, -5.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.rectLvl = NO_RECT") {
    DANT::AtOCROpts opts;
    opts.rectLvl = DANT::NO_RECT;
    rack::simd::float_4 inSignals = rack::simd::float_4(2.5f, 5.5f, -2.5f, -5.5f);
    rack::simd::float_4 expected = rack::simd::float_4(2.5f, 5.5f, -2.5f, -5.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.rectLvl = HALF_RECT") {
    DANT::AtOCROpts opts;
    opts.rectLvl = DANT::HALF_RECT;
    rack::simd::float_4 inSignals = rack::simd::float_4(2.5f, 5.5f, -2.5f, -5.5f);
    rack::simd::float_4 expected = rack::simd::float_4(2.5f, 5.5f, 0.0f, 0.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("opts.rectLvl = FULL_RECT") {
    DANT::AtOCROpts opts;
    opts.rectLvl = DANT::FULL_RECT;
    rack::simd::float_4 inSignals = rack::simd::float_4(2.5f, 5.5f, -2.5f, -5.5f);
    rack::simd::float_4 expected = rack::simd::float_4(2.5f, 5.5f, 2.5f, 5.5f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("Combo test 1, a 1.5, o -1.5, c 5, ao, half") {
    DANT::AtOCROpts opts;
    opts.attenuversion = 1.5f;
    opts.offset = -1.5f;
    opts.clipLvl = DANT::FIVE_CLIP;
    opts.opOrder = DANT::AT_THEN_OFF;
    opts.rectLvl = DANT::HALF_RECT;
    rack::simd::float_4 inSignals = rack::simd::float_4(-8.33f, 3.33f, -4.33f, 8.33f);
    rack::simd::float_4 expected = rack::simd::float_4(0.0f, 3.495f, 0.0f, 5.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }

  SECTION("Combo test 2, a -2, o 3, c 10, oa, full") {
    DANT::AtOCROpts opts;
    opts.attenuversion = -2.0f;
    opts.offset = 3.0f;
    opts.clipLvl = DANT::TEN_CLIP;
    opts.opOrder = DANT::OFF_THEN_AT;
    opts.rectLvl = DANT::FULL_RECT;
    rack::simd::float_4 inSignals = rack::simd::float_4(-2.33f, 2.33f, -4.33f, 4.33f);
    rack::simd::float_4 expected = rack::simd::float_4(1.34f, 10.0f, 2.66f, 10.0f);

    rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(inSignals, opts);

    require_float4_approx_equal(inSignals, outSignals, expected);
  }
}
