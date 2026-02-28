#include "plugin.hpp"

rack::plugin::Plugin* pluginInstance;

void init(rack::plugin::Plugin* p) {
  pluginInstance = p;

  p->addModel(modelAocr);
}

float DANT::PANEL_R_B{DANT::DEFAULT_R_B};
float DANT::PANEL_G_B{DANT::DEFAULT_G_B};
float DANT::PANEL_B_B{DANT::DEFAULT_B_B};
float DANT::PANEL_R_D{DANT::DEFAULT_R_D};
float DANT::PANEL_G_D{DANT::DEFAULT_G_D};
float DANT::PANEL_B_D{DANT::DEFAULT_B_D};

namespace DANT {
rack::math::Vec layout(const float column, const float row) {
  return rack::math::Vec(_X * ((column * 2.0f) - 1.0f), _Y * ((row * 2.0f) - 1.0f));
}
}  // namespace DANT
