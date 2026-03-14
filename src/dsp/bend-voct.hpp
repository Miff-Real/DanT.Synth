#pragma once

#include <rack.hpp>

#include "../static.hpp"

namespace DANT {

enum BEND_DIR { TOWARDS_PITCH, AWAY_FROM_PITCH };

struct BendOpts {
  rack::simd::float_4 startOffsets{0.0f};
  rack::simd::float_4 targetOffsets{0.0f};
  rack::simd::float_4 progress{0.0f};
  rack::simd::float_4 shape{0.0f};
  rack::simd::float_4 isUnbending{0.0f};
  bool inverseUnbend{false};

  BendOpts() = default;
};

inline rack::simd::float_4 bendVoct(const rack::simd::float_4 inSignals, BendOpts opts) {
  rack::simd::float_4 p = rack::simd::clamp(opts.progress, 0.0f, 1.0f);

  // Exponent derived from shape: pow(2, shape * 2.0)
  // shape = -1 (log) -> exp = 0.25
  // shape = 0 (lin) -> exp = 1.0
  // shape = 1 (exp) -> exp = 4.0
  rack::simd::float_4 exponents = rack::dsp::exp2_taylor5(opts.shape * 2.0f);

  rack::simd::float_4 curved = rack::simd::ifelse(p > 0.0f, rack::simd::pow(p, exponents), 0.0f);

  if (opts.inverseUnbend) {
    // If we want a symmetric "hill", the return curve should be inverted in time.
    // E.g., if bend was "fast then slow" (log, e < 1), unbend should be "slow then fast".
    // We achieve an exact time-domain mirror reflection by running the progress backward:
    // f(p) = 1.0 - (1.0 - p)^e
    rack::simd::float_4 p_inv = 1.0f - p;
    rack::simd::float_4 unbendCurve = 1.0f - rack::simd::ifelse(p_inv > 0.0f, rack::simd::pow(p_inv, exponents), 0.0f);
    curved = rack::simd::ifelse(opts.isUnbending != 0.0f, unbendCurve, curved);
  }

  rack::simd::float_4 offsets = opts.startOffsets + (opts.targetOffsets - opts.startOffsets) * curved;

  return inSignals + offsets;
}

}  // namespace DANT
