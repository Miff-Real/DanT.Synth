#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>  // std::acos
#include <rack.hpp>

namespace DANT {

static const double PI = std::acos(-1.0);
static const int CHANS{16};                                                      // used for array sizing
static const int SIMD{4};                                                        // used for simd looping
static const int SIMD_I[CHANS]{0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};  // float[16] index to float_4[4] index
static const int SIMD_J[CHANS]{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};  // channel index to float_4 index
static const rack::simd::float_4 SIMD_ZERO{0.0f, 0.0f, 0.0f, 0.0f};

static const rack::simd::float_4 R_ZERO{0.0f};
static const rack::simd::float_4 M_TEN{-10.0f};
static const rack::simd::float_4 P_TEN{10.0f};
static const rack::simd::float_4 M_FIVE{-5.0f};
static const rack::simd::float_4 P_FIVE{5.0f};

}  // namespace DANT
