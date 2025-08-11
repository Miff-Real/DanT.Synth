#pragma once

#ifndef _USE_MATH_DEFINES
#define _USE_MATH_DEFINES
#endif

#include <cmath>  // std::acos
#include <rack.hpp>
#include <string>

extern rack::plugin::Plugin *pluginInstance;

extern rack::plugin::Model *modelAocr;

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

// Plugin shared settings
static const std::string PLUGIN_SETTINGS_FILENAME{"DanTSynth.json"};

static void saveSettings(json_t *rootJ) {
  std::string settingsFilename = rack::asset::user(DANT::PLUGIN_SETTINGS_FILENAME);
  FILE *file = fopen(settingsFilename.c_str(), "w");
  if (file) {
    json_dumpf(rootJ, file, JSON_INDENT(2) | JSON_REAL_PRECISION(9));
    fclose(file);
  }
}

static json_t *readSettings() {
  std::string settingsFilename = rack::asset::user(DANT::PLUGIN_SETTINGS_FILENAME);
  FILE *file = fopen(settingsFilename.c_str(), "r");
  if (!file) {
    return json_object();
  }
  json_error_t error;
  json_t *rootJ = json_loadf(file, 0, &error);
  fclose(file);
  return rootJ;
}

static void saveUserSettings() {
  json_t *rootJ = json_object();

  json_object_set_new(rootJ, "panelBrightRed", json_integer(static_cast<int>(DANT::PANEL_R_B)));
  json_object_set_new(rootJ, "panelBrightGreen", json_integer(static_cast<int>(DANT::PANEL_G_B)));
  json_object_set_new(rootJ, "panelBrightBlue", json_integer(static_cast<int>(DANT::PANEL_B_B)));
  json_object_set_new(rootJ, "panelDarkRed", json_integer(static_cast<int>(DANT::PANEL_R_D)));
  json_object_set_new(rootJ, "panelDarkGreen", json_integer(static_cast<int>(DANT::PANEL_G_D)));
  json_object_set_new(rootJ, "panelDarkBlue", json_integer(static_cast<int>(DANT::PANEL_B_D)));

  DANT::saveSettings(rootJ);
}

static void loadUserSettings() {
  json_t *settingsJ{DANT::readSettings()};

  json_t *pbrJ = json_object_get(settingsJ, "panelBrightRed");
  json_t *pbgJ = json_object_get(settingsJ, "panelBrightGreen");
  json_t *pbbJ = json_object_get(settingsJ, "panelBrightBlue");
  json_t *pdrJ = json_object_get(settingsJ, "panelDarkRed");
  json_t *pdgJ = json_object_get(settingsJ, "panelDarkGreen");
  json_t *pdbJ = json_object_get(settingsJ, "panelDarkBlue");

  DANT::PANEL_R_B = pbrJ ? static_cast<float>(json_integer_value(pbrJ)) : DANT::DEFAULT_R_B;
  DANT::PANEL_G_B = pbgJ ? static_cast<float>(json_integer_value(pbgJ)) : DANT::DEFAULT_G_B;
  DANT::PANEL_B_B = pbbJ ? static_cast<float>(json_integer_value(pbbJ)) : DANT::DEFAULT_B_B;
  DANT::PANEL_R_D = pdrJ ? static_cast<float>(json_integer_value(pdrJ)) : DANT::DEFAULT_R_D;
  DANT::PANEL_G_D = pdgJ ? static_cast<float>(json_integer_value(pdgJ)) : DANT::DEFAULT_G_D;
  DANT::PANEL_B_D = pdbJ ? static_cast<float>(json_integer_value(pdbJ)) : DANT::DEFAULT_B_D;
}

}  // namespace DANT
