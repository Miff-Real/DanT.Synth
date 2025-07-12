#pragma once

#include <cmath>  // std::acos
#include <rack.hpp>

extern rack::plugin::Plugin *pluginInstance;

extern rack::plugin::Model *modelAtocr;

/**
 * Layout variables, _X is half a HP. Add widgets centred at x position HP * 2 - 1.
 * _Y is half of a 1/16th row. Add widgets centred at y position Row * 2 - 1.
 */
static const float _X = rack::app::RACK_GRID_WIDTH * 0.5f;
static const float _Y = rack::app::RACK_GRID_HEIGHT / 32.0f;

namespace DANT {
static const double PI = std::acos(-1.0);
static const int CHANS{16};                                                      // used for array sizing
static const int SIMD{4};                                                        // used for simd looping
static const int SIMD_I[CHANS]{0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3};  // float[16] index to float_4[4] index
static const int SIMD_J[CHANS]{0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3};  // channel index to float_4 index
static const rack::simd::float_4 SIMD_ZERO{0.0f, 0.0f, 0.0f, 0.0f};

static rack::math::Vec layout(const float column, const float row) {
  return rack::math::Vec(_X * ((column * 2.0f) - 1.0f), _Y * ((row * 2.0f) - 1.0f));
}

// Plugin shared variables
extern float PANEL_R_B;  // panel red bright
extern float PANEL_G_B;  // panel green bright
extern float PANEL_B_B;  // panel blue bright
extern float PANEL_R_D;  // panel red dark
extern float PANEL_G_D;  // panel green dark
extern float PANEL_B_D;  // panel blue dark

// Defaults
static const float DEFAULT_R_B{238.0f};
static const float DEFAULT_G_B{238.0f};
static const float DEFAULT_B_B{238.0f};
static const float DEFAULT_R_D{48.0f};
static const float DEFAULT_G_D{48.0f};
static const float DEFAULT_B_D{48.0f};

}  // namespace DANT
