#pragma once

#include "../plugin.hpp"

namespace DANT {
static const rack::simd::float_4 R_ZERO{0.0f};
static const rack::simd::float_4 M_TEN{-10.0f};
static const rack::simd::float_4 P_TEN{10.0f};
static const rack::simd::float_4 M_FIVE{-5.0f};
static const rack::simd::float_4 P_FIVE{5.0f};

/**
 * Clip (C) and Rectify (R) are universally commutative.
 * C(R(x))=R(C(x))
 * There are 4! = 24 different operational orders, but only 12 distinct functions.
 * The 12 orders in which R comes before C are omitted.
 */
enum OP_ORDER {
  AOCR,  // == AORC
  ACOR,  // == AROC
  ACRO,  // == ARCO
  OACR,  // == OARC
  OCAR,  // == ORAC
  OCRA,  // == ORCA
  CAOR,  // == RAOC
  CARO,  // == RACO
  COAR,  // == ROAC
  CORA,  // == ROCA
  CRAO,  // == RCAO
  CROA,  // == RCOA
};

enum CLIP_LVL { NO_CLIP, TEN_CLIP, FIVE_CLIP };

enum RECT_LVL { NO_RECT, HALF_RECT, FULL_RECT };

enum RECT_TYPE { POS_RECT, NEG_RECT };

struct AOCROpts {
  OP_ORDER opOrder = AOCR;
  float attenuversion = 1.0f;
  float offset = 0.0f;
  CLIP_LVL clipLvl = NO_CLIP;
  RECT_LVL rectLvl = NO_RECT;
  RECT_TYPE rectType = POS_RECT;

  AOCROpts() = default;

  AOCROpts(float a) : attenuversion(a) {}

  AOCROpts(float a, float o) : attenuversion(a), offset(o) {}

  AOCROpts(CLIP_LVL c) : clipLvl(c) {}

  AOCROpts(RECT_LVL r, RECT_TYPE rt) : rectLvl(r), rectType(rt) {}

  AOCROpts(OP_ORDER oo, float a, float o, CLIP_LVL c, RECT_LVL r, RECT_TYPE rt)
      : opOrder(oo), attenuversion(a), offset(o), clipLvl(c), rectLvl(r), rectType(rt) {}
};

inline rack::simd::float_4 doA(const rack::simd::float_4 signals, const float attenuversion) {
  return signals * attenuversion;
}

inline rack::simd::float_4 doO(const rack::simd::float_4 signals, const float offset) { return signals + offset; }

inline rack::simd::float_4 doC(const rack::simd::float_4 signals, const CLIP_LVL clipLvl) {
  switch (clipLvl) {
    case TEN_CLIP:
      return rack::simd::clamp(signals, M_TEN, P_TEN);
      break;
    case FIVE_CLIP:
      return rack::simd::clamp(signals, M_FIVE, P_FIVE);
      break;
    default:
      return signals;
      break;
  }
}

inline rack::simd::float_4 doR(const rack::simd::float_4 signals, const RECT_LVL rectLvl, const bool inverted = false) {
  switch (rectLvl) {
    case HALF_RECT:
      if (inverted) {
        return rack::simd::fmin(R_ZERO, signals);
      } else {
        return rack::simd::fmax(R_ZERO, signals);
      }
      break;
    case FULL_RECT:
      if (inverted) {
        return rack::simd::abs(signals) * -1.0f;
      } else {
        return rack::simd::abs(signals);
      }
      break;
    default:
      return signals;
      break;
  }
}

rack::simd::float_4 attenuvertOffsetClipRectify(const rack::simd::float_4 inSignals, AOCROpts opts) {
  rack::simd::float_4 outSignals{inSignals};
  switch (opts.opOrder) {
    case AOCR:
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case ACOR:
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case ACRO:
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doO(outSignals, opts.offset);
      break;
    case OACR:
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case OCAR:
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case OCRA:
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      break;
    case CAOR:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case CARO:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doO(outSignals, opts.offset);
      break;
    case COAR:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      break;
    case CORA:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      break;
    case CRAO:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      outSignals = DANT::doO(outSignals, opts.offset);
      break;
    case CROA:
      outSignals = DANT::doC(outSignals, opts.clipLvl);
      outSignals = DANT::doR(outSignals, opts.rectLvl, opts.rectType == NEG_RECT);
      outSignals = DANT::doO(outSignals, opts.offset);
      outSignals = DANT::doA(outSignals, opts.attenuversion);
      break;
    default:
      break;
  }
  return outSignals;
}

}  // namespace DANT
