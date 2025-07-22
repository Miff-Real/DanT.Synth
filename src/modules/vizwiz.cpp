#include "../plugin.hpp"
#include "../shared/module-widget.hpp"
#include "../shared/skiff-widget.hpp"

static bool masterVizWizExists{false};

const int HP{5};

struct VizWizModule : rack::engine::Module {
  bool thisIsMasterVizWizInstance{false};

  enum ParamIds { NUM_PARAMS };
  enum InputIds { NUM_INPUTS };
  enum OutputIds { NUM_OUTPUTS };
  enum LightIds { NUM_LIGHTS };

  VizWizModule() {
    if (!masterVizWizExists) {
      masterVizWizExists = true;
      thisIsMasterVizWizInstance = true;
    }

    rack::engine::Module::config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
  }

  ~VizWizModule() {
    if (thisIsMasterVizWizInstance) {
      masterVizWizExists = false;
    }
  }

  inline bool hasMasterAuthority() { return thisIsMasterVizWizInstance || rack::settings::isPlugin; }

  json_t *dataToJson() override {
    DANT::saveUserSettings();
    json_t *rootJ = json_object();
    return rootJ;
  }

  void dataFromJson(json_t *rootJ) override { DANT::loadUserSettings(); }

  void onReset() override {
    if (!this->hasMasterAuthority()) {
      return;
    }
    softReset();
    rack::engine::Module::onReset();
  }

  void softReset() {}

  void process(const rack::engine::Module::ProcessArgs &args) override {
    if (!this->hasMasterAuthority()) {
      return;
    }
  }
};

struct VizWizWidget : DANT::ModuleWidget {
  DANT::SkiffWidget *skiffWidget = nullptr;

  std::string moduleName() override { return "VizWiz"; }

  VizWizWidget(VizWizModule *module) {
    rack::app::ModuleWidget::setModule(module);
    this->box.size = rack::math::Vec(rack::app::RACK_GRID_WIDTH * HP, rack::app::RACK_GRID_HEIGHT);
    if (module) {
      if (!module->hasMasterAuthority()) {
        return;
      }
      DANT::SkiffOpts skiffOptions;
      this->skiffWidget = new DANT::SkiffWidget(this, skiffOptions);
      this->skiffWidget->setActive(true);
      auto railWidget = std::find_if(
          APP->scene->rack->children.begin(), APP->scene->rack->children.end(),
          [=](rack::widget::Widget *widget) { return dynamic_cast<rack::app::RailWidget *>(widget) != nullptr; });
      if (railWidget != APP->scene->rack->children.end()) {
        APP->scene->rack->addChildAbove(this->skiffWidget, *railWidget);
        DEBUG("Added skiff widget above Rack railWidget.");
      } else {
        WARN("VizWiz was unable to locate the Rack railWidget.");
      }
    }
  }

  ~VizWizWidget() {
    if (!APP->scene) {
      return;
    }
    if (!APP->scene->rack) {
      return;
    }
    if (this->skiffWidget) {
      APP->scene->rack->removeChild(this->skiffWidget);
    }
  }

  void draw(const rack::widget::Widget::DrawArgs &args) override { DANT::ModuleWidget::draw(args); }
};

rack::plugin::Model *modelVizWiz = rack::createModel<VizWizModule, VizWizWidget>("VizWiz");
