#pragma once

#include <cmath>  // For std::acos
#include <rack.hpp>

extern rack::plugin::Plugin *pluginInstance;

extern rack::plugin::Model *modelDanTTemplate;

namespace DANT {
static const double PI = std::acos(-1.0);
}
