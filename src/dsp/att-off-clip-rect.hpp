#pragma once

#include "../plugin.hpp"

namespace DANT {
static const rack::simd::float_4 R_ZERO{0.0f};
static const rack::simd::float_4 M_TEN{-10.0f};
static const rack::simd::float_4 P_TEN{10.0f};
static const rack::simd::float_4 M_FIVE{-5.0f};
static const rack::simd::float_4 P_FIVE{5.0f};

enum CLIP_LVL { NO_CLIP, TEN_CLIP, FIVE_CLIP };

enum OP_ORDER { AT_THEN_OFF, OFF_THEN_AT };

enum RECT_LVL { NO_RECT, HALF_RECT, FULL_RECT };

struct AtOCROpts {
  float attenuversion = 1.0f;
  float offset = 0.0f;
  CLIP_LVL clipLvl = NO_CLIP;
  OP_ORDER opOrder = AT_THEN_OFF;
  RECT_LVL rectLvl = NO_RECT;

  AtOCROpts() = default;
};

rack::simd::float_4 attenuvertOffsetClipRectify(const rack::simd::float_4 inSignals, AtOCROpts opts) {
  rack::simd::float_4 outSignals{inSignals};
  switch (opts.opOrder) {
    case AT_THEN_OFF:
      outSignals *= opts.attenuversion;
      outSignals += opts.offset;
      break;
    case OFF_THEN_AT:
      outSignals += opts.offset;
      outSignals *= opts.attenuversion;
      break;
    default:
      // do nothing
      break;
  }
  switch (opts.rectLvl) {
    case HALF_RECT:
      outSignals = rack::simd::fmax(R_ZERO, outSignals);
      break;
    case FULL_RECT:
      outSignals = rack::simd::abs(outSignals);
      break;
    default:
      // do nothing
      break;
  }
  switch (opts.clipLvl) {
    case TEN_CLIP:
      outSignals = rack::simd::clamp(outSignals, M_TEN, P_TEN);
      break;
    case FIVE_CLIP:
      outSignals = rack::simd::clamp(outSignals, M_FIVE, P_FIVE);
      break;
    default:
      // do nothing
      break;
  }
  return outSignals;
}
}  // namespace DANT
