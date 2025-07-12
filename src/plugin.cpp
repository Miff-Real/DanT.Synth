#include "plugin.hpp"

rack::plugin::Plugin *pluginInstance;

void init(rack::plugin::Plugin *p) {
  pluginInstance = p;

  p->addModel(modelAtocr);
}

float DANT::PANEL_R_B{DANT::DEFAULT_R_B};
float DANT::PANEL_G_B{DANT::DEFAULT_G_B};
float DANT::PANEL_B_B{DANT::DEFAULT_B_B};
float DANT::PANEL_R_D{DANT::DEFAULT_R_D};
float DANT::PANEL_G_D{DANT::DEFAULT_G_D};
float DANT::PANEL_B_D{DANT::DEFAULT_B_D};
