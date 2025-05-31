#pragma once

#include <cmath>  // std::acos
#include <rack.hpp>

extern rack::plugin::Plugin *pluginInstance;

extern rack::plugin::Model *modelDanTTemplate;

/**
 * Layout variables, _X is half a HP. Add widgets centred at x position HP * 2 - 1.
 * _Y is half of a 1/16th row. Add widgets centred at y position Row * 2 - 1.
 */
static const float _X = rack::app::RACK_GRID_WIDTH * 0.5f;
static const float _Y = rack::app::RACK_GRID_HEIGHT / 32.0f;

namespace DANT {
static const double PI = std::acos(-1.0);

static rack::math::Vec layout(const float column, const float row) {
  return rack::math::Vec(_X * ((column * 2.0f) - 1.0f), _Y * ((row * 2.0f) - 1.0f));
}
}  // namespace DANT
