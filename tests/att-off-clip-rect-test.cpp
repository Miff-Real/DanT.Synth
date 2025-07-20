#include "../src/dsp/att-off-clip-rect.hpp"

#include <rack.hpp>
#include <string>
#include <vector>

#include "catch2/catch.hpp"

const float FP_TOLERANCE = 1e-6f;

void check_float4_approx_equal(const rack::simd::float_4& input, const rack::simd::float_4& actual,
                               const rack::simd::float_4& expected) {
  for (int i{0}; i < 4; ++i) {
    Catch::Detail::Approx target = Catch::Detail::Approx(expected[i]).epsilon(FP_TOLERANCE);

    UNSCOPED_INFO("input [" << input[i] << "]");
    CHECK(actual[i] == target);
  }
}

struct TestCaseParams {
  std::string name;
  DANT::AOCROpts opts;
  rack::simd::float_4 inSignals;
  rack::simd::float_4 expectedSignals;
};

TEST_CASE("att-off-clip-rect.hpp::attenuvertOffsetClipRectify") {
  std::vector<TestCaseParams> testSuite = {
      {
          "Default options",
          DANT::AOCROpts(),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
      },
      {
          "attenuversion = 0.0x",
          DANT::AOCROpts(0.0f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(0.0f, 0.0f, 0.0f, 0.0f),
      },
      {
          "attenuversion = 0.5x",
          DANT::AOCROpts(0.5f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(0.5f, -1.0f, 0.0f, 2.6665f),
      },
      {
          "attenuversion = 2.0x",
          DANT::AOCROpts(2.0f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(2.0f, -4.0f, 0.0f, 10.666f),
      },
      {
          "attenuversion = -1.0x",
          DANT::AOCROpts(-1.0f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(-1.0f, 2.0f, 0.0f, -5.333f),
      },
      {
          "offset = 2.5",
          DANT::AOCROpts(1.0f, 2.5f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(3.5f, 0.5f, 2.5f, 7.833f),
      },
      {
          "offset = -6.6",
          DANT::AOCROpts(1.0f, -6.6f),
          rack::simd::float_4(1.0f, -2.0f, 0.0f, 5.333f),
          rack::simd::float_4(-5.6f, -8.6f, -6.6f, -1.267f),
      },
      {
          "clipLvl = TEN_CLIP",
          DANT::AOCROpts(DANT::CLIP_LVL::TEN_CLIP),
          rack::simd::float_4(1.0f, -2.0f, 11.5f, -12.0f),
          rack::simd::float_4(1.0f, -2.0f, 10.0f, -10.0f),
      },
      {
          "clipLvl = FIVE_CLIP",
          DANT::AOCROpts(DANT::CLIP_LVL::FIVE_CLIP),
          rack::simd::float_4(1.0f, -2.0f, 6.5f, -7.0f),
          rack::simd::float_4(1.0f, -2.0f, 5.0f, -5.0f),
      },
      {
          "rectLvl = HALF_RECT",
          DANT::AOCROpts(DANT::RECT_LVL::HALF_RECT, DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(1.0f, -2.0f, 6.5f, -7.0f),
          rack::simd::float_4(1.0f, 0.0f, 6.5f, 0.0f),
      },
      {
          "rectLvl = FULL_RECT",
          DANT::AOCROpts(DANT::RECT_LVL::FULL_RECT, DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(1.0f, -2.0f, 6.5f, -7.0f),
          rack::simd::float_4(1.0f, 2.0f, 6.5f, 7.0f),
      },
      {
          "rectLvl = HALF_RECT, rectType = NEG_RECT",
          DANT::AOCROpts(DANT::RECT_LVL::HALF_RECT, DANT::RECT_TYPE::NEG_RECT),
          rack::simd::float_4(1.0f, -2.0f, 6.5f, -7.0f),
          rack::simd::float_4(0.0f, -2.0f, 0.0f, -7.0f),
      },
      {
          "rectLvl = FULL_RECT, rectType = NEG_RECT",
          DANT::AOCROpts(DANT::RECT_LVL::FULL_RECT, DANT::RECT_TYPE::NEG_RECT),
          rack::simd::float_4(1.0f, -2.0f, 6.5f, -7.0f),
          rack::simd::float_4(-1.0f, -2.0f, -6.5f, -7.0f),
      },
      {
          "opOrder = AOCR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::AOCR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(5.0f, 5.0f, 4.0f, 0.0f),
      },
      {
          "opOrder = ACOR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::ACOR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(10.0f, 6.0f, 4.0f, 0.0f),
      },
      {
          "opOrder = ACRO, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::ACRO, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(10.0f, 6.0f, 5.0f, 5.0f),
      },
      {
          "opOrder = OACR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::OACR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(3.5f, 0.0f, 0.0f, 0.0f),
      },
      {
          "opOrder = OCAR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::OCAR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(2.5f, 0.0f, 0.0f, 0.0f),
      },
      {
          "opOrder = OCRA, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::OCRA, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(0.0f, -1.5f, -2.5f, -2.5f),
      },
      {
          "opOrder = CAOR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::CAOR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(7.5f, 6.0f, 4.0f, 2.5f),
      },
      {
          "opOrder = CARO, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::CARO, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(7.5f, 6.0f, 5.0f, 5.0f),
      },
      {
          "opOrder = COAR, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::COAR, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(0.0f, 0.0f, 0.0f, 0.0f),
      },
      {
          "opOrder = CORA, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::CORA, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(0.0f, -1.5f, -3.5f, -5.0f),
      },
      {
          "opOrder = CRAO, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::CRAO, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(5.0f, 5.0f, 4.0f, 2.5f),
      },
      {
          "opOrder = CROA, a = -0.5, o = 5.0, c = FIVE_CLIP, r = HALF_RECT",
          DANT::AOCROpts(DANT::OP_ORDER::CROA, -0.5f, 5.0f, DANT::CLIP_LVL::FIVE_CLIP, DANT::RECT_LVL::HALF_RECT,
                         DANT::RECT_TYPE::POS_RECT),
          rack::simd::float_4(-12.0f, -2.0f, 2.0f, 12.0f),
          rack::simd::float_4(-2.5f, -2.5f, -3.5f, -5.0f),
      },
  };

  for (const auto& testCase : testSuite) {
    SECTION(testCase.name) {
      rack::simd::float_4 outSignals = DANT::attenuvertOffsetClipRectify(testCase.inSignals, testCase.opts);

      check_float4_approx_equal(testCase.inSignals, outSignals, testCase.expectedSignals);
    }
  }
}
