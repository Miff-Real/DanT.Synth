#pragma once

#include <rack.hpp>

namespace DANT {
enum RGBcolour { RGB_R, RGB_G, RGB_B, RGB_A, RGB_LENGTH };

struct MenuSlider : rack::ui::Slider {
  MenuSlider(rack::Quantity *quant, const float width) {
    this->box.size.x = width;
    this->quantity = quant;
  }
  ~MenuSlider() { delete this->quantity; }
};

struct RGBValueQuantity : rack::Quantity {
  const std::string labels[RGB_LENGTH]{"Red", "Green", "Blue", "Alpha"};
  const float rgbMin{0.0f};
  const float rgbMax{255.0f};
  const float rgbDefault[RGB_LENGTH]{0.0f, 0.0f, 0.0f, 255.0f};

  RGBcolour valType;
  float *srcRange = NULL;

  RGBValueQuantity(const RGBcolour _valType, float *_srcRange) {
    this->valType = _valType;
    this->srcRange = _srcRange;
  }

  void setValue(float value) override { *srcRange = rack::math::clamp(value, getMinValue(), getMaxValue()); }
  float getValue() override { return *srcRange; }
  float getMinValue() override { return rgbMin; }
  float getMaxValue() override { return rgbMax; }
  float getDefaultValue() override { return rgbDefault[static_cast<int>(valType)]; }
  float getDisplayValue() override { return getValue(); }
  std::string getDisplayValueString() override { return rack::string::f("%d", static_cast<int>(getDisplayValue())); }
  void setDisplayValue(float displayValue) override { setValue(displayValue); }
  std::string getLabel() override { return labels[static_cast<int>(valType)]; }
};
}  // namespace DANT
