#pragma once

#include <osdialog.h>

#include <algorithm>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "../plugin.hpp"
#include "skiffs.hpp"

namespace DANT {

struct SkiffWidget : rack::widget::TransparentWidget {
  rack::math::Vec screenTL;
  rack::math::Vec screenBR;
  rack::math::Vec screenCentre;

  NVGcolor backgroundColour;
  NVGcolor gradientColour;
  NVGpaint gradientPaint;
  NVGcolor skiffBezelColour;
  NVGcolor skiffSideColour;
  NVGcolor skiffDepthColour;
  NVGcolor skiffInsideColour;
  NVGcolor skiffBackColour;
  NVGcolor skiffInsideSideColour;
  NVGpaint skiffRailsImagePaint;
  NVGpaint userSelectedImagePaint;

  rack::app::ModuleWidget *vizwiz = nullptr;
  bool m_active{true};
  std::string m_selectedImageFilepath;

  const SkiffOpts m_opts;

  SkiffWidget(rack::app::ModuleWidget *_vizwiz, const SkiffOpts opts_param) : vizwiz(_vizwiz), m_opts(opts_param) {}

  void setActive(bool newActive) { m_active = newActive; }

  void setSelectedImageFilepath(const std::string &newPath) { m_selectedImageFilepath = newPath; }

  void draw(const rack::widget::Widget::DrawArgs &args) override {
    if (m_active) {
      doRender(args);
    }
    TransparentWidget::draw(args);
  }

  void doRender(const rack::widget::Widget::DrawArgs &args) {
    screenTL = args.clipBox.getTopLeft();
    screenBR = args.clipBox.getBottomRight();
    screenCentre = args.clipBox.getCenter();

    if (m_opts.drawBackgroundColour) {
      backgroundColour =
          nvgRGBA(static_cast<int>(m_opts.backgroundColourR), static_cast<int>(m_opts.backgroundColourG),
                  static_cast<int>(m_opts.backgroundColourB), static_cast<int>(m_opts.backgroundColourA));

      gradientColour = nvgRGBA(static_cast<int>(m_opts.gradientColourR), static_cast<int>(m_opts.gradientColourG),
                               static_cast<int>(m_opts.gradientColourB), static_cast<int>(m_opts.gradientColourA));

      gradientPaint = nvgLinearGradient(args.vg, screenCentre.x, screenTL.y, screenCentre.x, screenBR.y,
                                        backgroundColour, gradientColour);
    }

    if (m_opts.drawBackgroundColour && m_opts.imageOntop) {
      renderBackgroundColour(args, m_opts.gradientMode);
    }

    if (m_opts.drawBackgroundImage && m_selectedImageFilepath != "") {
      std::shared_ptr<rack::window::Image> userSelectedImage = APP->window->loadImage(m_selectedImageFilepath);

      if (userSelectedImage != nullptr) {
        int iImgWidth;
        int iImgHeight;
        nvgImageSize(args.vg, userSelectedImage->handle, &iImgWidth, &iImgHeight);
        float imgWidth{static_cast<float>(iImgWidth)};
        float imgHeight{static_cast<float>(iImgHeight)};

        if (m_opts.backgroundScale != 1.0f) {
          imgWidth = imgWidth * m_opts.backgroundScale;
          imgHeight = imgHeight * m_opts.backgroundScale;
        }

        if (m_opts.ignoreZoom) {
          float transform[6];
          nvgCurrentTransform(args.vg, transform);
          // The zoom factor is the inverse of the y-scale component of the transform matrix.
          float zoom = 1.0f / transform[3];
          imgWidth = imgWidth * zoom;
          imgHeight = imgHeight * zoom;
        }

        rack::math::Vec imgPos;
        switch (m_opts.imagePosition) {
          case TOP_LEFT:
            imgPos = rack::math::Vec(screenTL.x, screenTL.y);
            break;
          case TOP_CENTRE:
            imgPos = rack::math::Vec(screenCentre.x - (imgWidth * 0.5f), screenTL.y);
            break;
          case TOP_RIGHT:
            imgPos = rack::math::Vec(screenBR.x - imgWidth, screenTL.y);
            break;
          case MIDDLE_LEFT:
            imgPos = rack::math::Vec(screenTL.x, screenCentre.y - (imgHeight * 0.5f));
            break;
          case MIDDLE_CENTRE:
            imgPos = rack::math::Vec(screenCentre.x - (imgWidth * 0.5f), screenCentre.y - (imgHeight * 0.5f));
            break;
          case MIDDLE_RIGHT:
            imgPos = rack::math::Vec(screenBR.x - imgWidth, screenCentre.y - (imgHeight * 0.5f));
            break;
          case BOTTOM_LEFT:
            imgPos = rack::math::Vec(screenTL.x, screenBR.y - imgHeight);
            break;
          case BOTTOM_CENTRE:
            imgPos = rack::math::Vec(screenCentre.x - (imgWidth * 0.5f), screenBR.y - imgHeight);
            break;
          case BOTTOM_RIGHT:
            imgPos = rack::math::Vec(screenBR.x - imgWidth, screenBR.y - imgHeight);
            break;
          default:
            imgPos = rack::math::Vec(screenCentre.x - (imgWidth * 0.5f), screenCentre.y - (imgHeight * 0.5f));
            break;
        }

        if (m_opts.xPositionOffset != 0.0f) {
          imgPos.x += m_opts.xPositionOffset * args.clipBox.size.x;
        }
        if (m_opts.yPositionOffset != 0.0f) {
          imgPos.y += m_opts.yPositionOffset * args.clipBox.size.y;
        }

        rack::math::Vec imgTL{imgPos};

        if (m_opts.xImageOffset != 0.0f) {
          imgTL.x += m_opts.xImageOffset * imgWidth;
        }
        if (m_opts.yImageOffset != 0.0f) {
          imgTL.y += m_opts.yImageOffset * imgHeight;
        }

        userSelectedImagePaint = nvgImagePattern(args.vg, imgTL.x, imgTL.y, imgWidth, imgHeight,
                                                 0.0f,  // Angle
                                                 userSelectedImage->handle,
                                                 1.0f);  // Alpha

        renderBackgroundImage(args, imgPos, m_opts.tileImage, imgWidth, imgHeight);
      }
    }

    if (m_opts.drawBackgroundColour && !m_opts.imageOntop) {
      renderBackgroundColour(args, m_opts.gradientMode);
    }

    int bezelSize{m_opts.drawSkiffBezel ? static_cast<int>(m_opts.skiffBezelSize) : 0};
    float skiffDepthY{m_opts.drawSkiff ? (static_cast<int>(m_opts.skiffDepth) * rack::app::RACK_GRID_WIDTH) : 0.0f};
    float skiffDepthX{skiffDepthY * 0.5f};  // X depth is half of Y depth for perspective.

    std::vector<rack::math::Rect> skiffBoxes = DANT::CalculateSkiffBoxes(
        this->vizwiz->parent->children, this->vizwiz->parent->getVisibleChildrenBoundingBox(), m_opts);

    if (m_opts.drawSkiffShadow) {
      NVGcolor shadowColor = nvgRGBAf(0.0f, 0.0f, 0.0f, 0.1f + (0.4f * m_opts.skiffShadowIntensity));
      float offsetX{(bezelSize + bezelSize + skiffDepthX) * 2.0f * m_opts.skiffShadowSize};
      float offsetY{(bezelSize + bezelSize + skiffDepthY) * 2.0f * m_opts.skiffShadowSize};
      float blur{offsetX * 2.0f * m_opts.skiffShadowSoftness};
      for (rack::math::Rect sb : skiffBoxes) {
        renderShadow(args, sb, static_cast<float>(bezelSize), skiffDepthX, skiffDepthY, shadowColor, blur, offsetX,
                     offsetY);
      }
    }

    if (m_opts.drawSkiff) {
      skiffSideColour = nvgRGB(rack::math::clamp(static_cast<int>(m_opts.skiffColourR) - 30, 0, 255),
                               rack::math::clamp(static_cast<int>(m_opts.skiffColourG) - 30, 0, 255),
                               rack::math::clamp(static_cast<int>(m_opts.skiffColourB) - 30, 0, 255));
      skiffDepthColour = nvgRGB(rack::math::clamp(static_cast<int>(m_opts.skiffColourR) - 55, 0, 255),
                                rack::math::clamp(static_cast<int>(m_opts.skiffColourG) - 55, 0, 255),
                                rack::math::clamp(static_cast<int>(m_opts.skiffColourB) - 55, 0, 255));
      skiffInsideColour = nvgRGBA(rack::math::clamp(static_cast<int>(m_opts.internalColourR), 0, 255),
                                  rack::math::clamp(static_cast<int>(m_opts.internalColourG), 0, 255),
                                  rack::math::clamp(static_cast<int>(m_opts.internalColourB), 0, 255),
                                  rack::math::clamp(static_cast<int>(m_opts.internalColourA), 0, 255));
      skiffBackColour = nvgRGB(rack::math::clamp(static_cast<int>(m_opts.internalColourR) - 25, 0, 255),
                               rack::math::clamp(static_cast<int>(m_opts.internalColourG) - 25, 0, 255),
                               rack::math::clamp(static_cast<int>(m_opts.internalColourB) - 25, 0, 255));
      skiffInsideSideColour = nvgRGB(rack::math::clamp(static_cast<int>(m_opts.internalColourR) + 25, 0, 255),
                                     rack::math::clamp(static_cast<int>(m_opts.internalColourG) + 25, 0, 255),
                                     rack::math::clamp(static_cast<int>(m_opts.internalColourB) + 25, 0, 255));

      for (rack::math::Rect sb : skiffBoxes) {
        renderSkiff(args, sb, static_cast<float>(bezelSize), bezelSize * 0.5f, skiffDepthX, skiffDepthY);
      }
    }

    if (m_opts.drawRails) {
      std::shared_ptr<rack::window::Image> skiffRailsImg =
          APP->window->loadImage(rack::asset::plugin(pluginInstance, "res/skiffrails.png"));

      skiffRailsImagePaint = nvgImagePattern(args.vg, 0.0f, 0.0f, rack::app::RACK_GRID_WIDTH,
                                             rack::app::RACK_GRID_HEIGHT, 0.0f, skiffRailsImg->handle, 1.0f);

      for (auto sb : skiffBoxes) {
        renderRails(args, sb);
      }
    }

    if (m_opts.drawSkiffBezel) {
      skiffBezelColour = nvgRGB(static_cast<int>(m_opts.skiffColourR), static_cast<int>(m_opts.skiffColourG),
                                static_cast<int>(m_opts.skiffColourB));

      for (auto sb : skiffBoxes) {
        renderSkiffBezel(args, sb, static_cast<float>(bezelSize), bezelSize * 0.5f);
      }
    }
  }

  void renderBackgroundColour(const rack::widget::Widget::DrawArgs &args, const bool gradientMode) {
    DEBUG("SkiffWidget.renderBackgroundColour");
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    if (gradientMode) {
      nvgFillPaint(args.vg, gradientPaint);
    } else {
      nvgFillColor(args.vg, backgroundColour);
    }
    nvgRect(args.vg, screenTL.x, screenTL.y, screenBR.x, screenBR.y);
    nvgFill(args.vg);
    nvgRestore(args.vg);
  }

  void renderBackgroundImage(const rack::widget::Widget::DrawArgs &args, const rack::math::Vec imgPos, const bool tile,
                             const float iw, const float ih) {
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgFillPaint(args.vg, userSelectedImagePaint);
    if (tile) {
      nvgRect(args.vg, screenTL.x, screenTL.y, screenBR.x, screenBR.y);
    } else {
      nvgRect(args.vg, imgPos.x, imgPos.y, iw, ih);
    }
    nvgFill(args.vg);
    nvgRestore(args.vg);
  }

  void renderShadow(const rack::widget::Widget::DrawArgs &args, const rack::math::Rect skiffDimensions,
                    const float bezelSize, const float depthX, const float depthY, NVGcolor shadowColor,
                    const float blur, const float offsetX, const float offsetY) {
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgScissor(
        args.vg, skiffDimensions.pos.x - skiffDimensions.size.x - bezelSize - bezelSize - depthX - offsetX,
        skiffDimensions.pos.y - bezelSize,
        (skiffDimensions.size.x + bezelSize + bezelSize + depthX + offsetX) +
            (skiffDimensions.size.x + bezelSize + depthX),
        (bezelSize) + (skiffDimensions.size.y + skiffDimensions.size.y + bezelSize + bezelSize + depthY + offsetY));

    nvgRect(args.vg, RECT_ARGS(rack::math::Rect(
                         skiffDimensions.pos.x - skiffDimensions.size.x - bezelSize - bezelSize - depthX - offsetX,
                         skiffDimensions.pos.y - bezelSize,
                         skiffDimensions.size.x + skiffDimensions.size.x + m_opts.skiffShadowSize + bezelSize +
                             bezelSize + depthX + offsetY,
                         skiffDimensions.size.y + skiffDimensions.size.y + bezelSize + bezelSize + depthY + offsetY)));

    nvgFillPaint(args.vg,
                 nvgBoxGradient(args.vg,
                                RECT_ARGS(rack::math::Rect(
                                    skiffDimensions.pos.x - bezelSize - offsetX, skiffDimensions.pos.y - bezelSize,
                                    skiffDimensions.size.x + bezelSize + bezelSize + depthX + offsetX,
                                    skiffDimensions.size.y + bezelSize + bezelSize + depthY + offsetY)),
                                blur * 4.0f,                         // Corner radius
                                blur * 2.0f,                         // Feather (blur amount)
                                shadowColor,                         // Inner colour
                                nvgRGBAf(0.0f, 0.0f, 0.0f, 0.0f)));  // Outer colour (transparent)
    nvgFill(args.vg);
    nvgRestore(args.vg);
  }

  void renderSkiff(const rack::widget::Widget::DrawArgs &args, const rack::math::Rect skiffDimensions, float bezelSize,
                   const float halfBezelSize, const float depthX, const float depthY) {
    nvgSave(args.vg);

    // Draw skiff depth (the top and left edges of the 3D effect).
    nvgBeginPath(args.vg);
    nvgFillColor(args.vg, skiffDepthColour);
    nvgMoveTo(args.vg, skiffDimensions.pos.x - bezelSize, skiffDimensions.pos.y - bezelSize);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize, skiffDimensions.pos.y - bezelSize);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize, skiffDimensions.pos.y + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + skiffDimensions.size.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize,
              skiffDimensions.pos.y + skiffDimensions.size.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize,
              skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize + depthX,
              skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX,
              skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x - bezelSize, skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    // Draw skiff side (the right edge of the 3D effect).
    nvgBeginPath(args.vg);
    nvgFillColor(args.vg, skiffSideColour);
    nvgMoveTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize, skiffDimensions.pos.y - bezelSize);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize + depthX,
              skiffDimensions.pos.y + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize + depthX,
              skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x + bezelSize,
              skiffDimensions.pos.y + skiffDimensions.size.y + bezelSize);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    // Draw skiff insides (the main flat area where modules sit).
    nvgBeginPath(args.vg);
    nvgFillColor(args.vg, skiffInsideColour);
    nvgRect(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + depthY, skiffDimensions.size.x - depthX,
            skiffDimensions.size.y - depthY);
    nvgFill(args.vg);

    // Draw skiff back (the top internal edge).
    nvgBeginPath(args.vg);
    nvgFillColor(args.vg, skiffBackColour);
    nvgMoveTo(args.vg, skiffDimensions.pos.x, skiffDimensions.pos.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x, skiffDimensions.pos.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x + skiffDimensions.size.x, skiffDimensions.pos.y + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + depthY);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    // Draw skiff inside side (the left internal edge).
    nvgBeginPath(args.vg);
    nvgFillColor(args.vg, skiffInsideSideColour);
    nvgMoveTo(args.vg, skiffDimensions.pos.x, skiffDimensions.pos.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + depthY);
    nvgLineTo(args.vg, skiffDimensions.pos.x + depthX, skiffDimensions.pos.y + skiffDimensions.size.y);
    nvgLineTo(args.vg, skiffDimensions.pos.x, skiffDimensions.pos.y + skiffDimensions.size.y);
    nvgClosePath(args.vg);
    nvgFill(args.vg);

    nvgRestore(args.vg);
  }

  void renderRails(const rack::widget::Widget::DrawArgs &args, const rack::math::Rect skiffDimensions) {
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgFillPaint(args.vg, skiffRailsImagePaint);
    nvgRect(args.vg, skiffDimensions.pos.x, skiffDimensions.pos.y, skiffDimensions.size.x, skiffDimensions.size.y);
    nvgFill(args.vg);
    nvgRestore(args.vg);
  }

  void renderSkiffBezel(const rack::widget::Widget::DrawArgs &args, const rack::math::Rect skiffDimensions,
                        const float bezelSize, const float halfBezelSize) {
    nvgSave(args.vg);
    nvgBeginPath(args.vg);
    nvgStrokeColor(args.vg, skiffBezelColour);
    nvgStrokeWidth(args.vg, bezelSize);
    nvgRect(args.vg, skiffDimensions.pos.x - halfBezelSize, skiffDimensions.pos.y - halfBezelSize,
            skiffDimensions.size.x + bezelSize, skiffDimensions.size.y + bezelSize);
    nvgStroke(args.vg);
    nvgRestore(args.vg);
  }
};

}  // namespace DANT
