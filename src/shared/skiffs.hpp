#pragma once

#include <algorithm>
#include <map>
#include <rack.hpp>
#include <set>
#include <string>
#include <vector>

namespace DANT {

// Constants related to skiff drawing, used for default values in SkiffOpts.
const float HORIZONTAL_SPLIT_MIN{1.0f};
const float HORIZONTAL_SPLIT_MAX{100.0f};
const float HORIZONTAL_SPLIT_THRESHOLD_DEFAULT{20.0f};

const bool DRAW_BACKGROUND_COLOUR_DEFAULT{true};
const bool GRADIENT_MODE_DEFAULT{true};
const bool DRAW_BACKGROUND_IMAGE_DEFAULT{true};
const bool IMAGE_ONTOP_DEFAULT{true};
const bool IGNORE_ZOOM_DEFAULT{false};
const bool TILE_IMAGE_DEFAULT{true};
const float BACKGROUND_SCALE_DEFAULT{1.0f};
const bool DRAW_SKIFF_DEFAULT{true};
const bool MULTIMODE_DEFAULT{false};
const bool SPLIT_VERTICALLY_DEFAULT{false};
const bool DRAW_SKIFF_BEZEL_DEFAULT{true};
const bool DRAW_RAILS_DEFAULT{true};
const bool DRAW_SKIFF_SHADOW_DEFAULT{true};

const float SKIFF_DEPTH_DEFAULT{2.0f};
const float SKIFF_BEZEL_SIZE_DEFAULT{2.0f};
const float SKIFF_SHADOW_SIZE_DEFAULT{0.4f};
const float SKIFF_SHADOW_SOFTNESS_DEFAULT{0.4f};
const float SKIFF_SHADOW_INTENSITY_DEFAULT{0.4f};

const float BACKGROUND_COLOUR_R_DEFAULT{175.0f};
const float BACKGROUND_COLOUR_G_DEFAULT{170.0f};
const float BACKGROUND_COLOUR_B_DEFAULT{167.0f};
const float BACKGROUND_COLOUR_A_DEFAULT{255.0f};

const float GRADIENT_COLOUR_R_DEFAULT{100.0f};
const float GRADIENT_COLOUR_G_DEFAULT{95.0f};
const float GRADIENT_COLOUR_B_DEFAULT{92.0f};
const float GRADIENT_COLOUR_A_DEFAULT{255.0f};

const float SKIFF_COLOUR_R_DEFAULT{125.0f};
const float SKIFF_COLOUR_G_DEFAULT{120.0f};
const float SKIFF_COLOUR_B_DEFAULT{113.0f};

const float INTERNAL_COLOUR_R_DEFAULT{50.0f};
const float INTERNAL_COLOUR_G_DEFAULT{45.0f};
const float INTERNAL_COLOUR_B_DEFAULT{38.0f};
const float INTERNAL_COLOUR_A_DEFAULT{255.0f};

/**
 * @brief Defines the possible positions for an image on the background.
 */
enum ImagePosition {
  TOP_LEFT,
  TOP_CENTRE,
  TOP_RIGHT,
  MIDDLE_LEFT,
  MIDDLE_CENTRE,
  MIDDLE_RIGHT,
  BOTTOM_LEFT,
  BOTTOM_CENTRE,
  BOTTOM_RIGHT
};
const ImagePosition IMAGE_POSITION_DEFAULT{MIDDLE_CENTRE};  // Default for ImagePosition

/**
 * @brief SkiffOpts struct holds all configurable drawing parameters for the SkiffWidget.
 * Each member has a sensible default value, allowing clients to override only what's necessary.
 */
struct SkiffOpts {
  bool drawBackgroundColour = DRAW_BACKGROUND_COLOUR_DEFAULT;
  float backgroundColourR = BACKGROUND_COLOUR_R_DEFAULT;
  float backgroundColourG = BACKGROUND_COLOUR_G_DEFAULT;
  float backgroundColourB = BACKGROUND_COLOUR_B_DEFAULT;
  float backgroundColourA = BACKGROUND_COLOUR_A_DEFAULT;
  float gradientColourR = GRADIENT_COLOUR_R_DEFAULT;
  float gradientColourG = GRADIENT_COLOUR_G_DEFAULT;
  float gradientColourB = GRADIENT_COLOUR_B_DEFAULT;
  float gradientColourA = GRADIENT_COLOUR_A_DEFAULT;
  bool gradientMode = GRADIENT_MODE_DEFAULT;
  bool drawBackgroundImage = DRAW_BACKGROUND_IMAGE_DEFAULT;
  bool imageOntop = IMAGE_ONTOP_DEFAULT;
  float backgroundScale = BACKGROUND_SCALE_DEFAULT;
  bool ignoreZoom = IGNORE_ZOOM_DEFAULT;
  int imagePosition = IMAGE_POSITION_DEFAULT;
  float xPositionOffset = 0.0f;
  float yPositionOffset = 0.0f;
  float xImageOffset = 0.0f;
  float yImageOffset = 0.0f;
  bool tileImage = TILE_IMAGE_DEFAULT;
  bool multiSkiffMode = MULTIMODE_DEFAULT;
  bool splitVertically = SPLIT_VERTICALLY_DEFAULT;
  float horizontalSplitThreshold = HORIZONTAL_SPLIT_THRESHOLD_DEFAULT;
  bool drawSkiffBezel = DRAW_SKIFF_BEZEL_DEFAULT;
  float skiffBezelSize = SKIFF_BEZEL_SIZE_DEFAULT;
  bool drawSkiff = DRAW_SKIFF_DEFAULT;
  float skiffDepth = SKIFF_DEPTH_DEFAULT;
  bool drawSkiffShadow = DRAW_SKIFF_SHADOW_DEFAULT;
  float skiffShadowSize = SKIFF_SHADOW_SIZE_DEFAULT;
  float skiffShadowSoftness = SKIFF_SHADOW_SOFTNESS_DEFAULT;
  float skiffShadowIntensity = SKIFF_SHADOW_INTENSITY_DEFAULT;
  float skiffColourR = SKIFF_COLOUR_R_DEFAULT;
  float skiffColourG = SKIFF_COLOUR_G_DEFAULT;
  float skiffColourB = SKIFF_COLOUR_B_DEFAULT;
  float internalColourR = INTERNAL_COLOUR_R_DEFAULT;
  float internalColourG = INTERNAL_COLOUR_G_DEFAULT;
  float internalColourB = INTERNAL_COLOUR_B_DEFAULT;
  float internalColourA = INTERNAL_COLOUR_A_DEFAULT;
  bool drawRails = DRAW_RAILS_DEFAULT;
};

/**
 * @brief Calculates the bounding boxes for the skiff(s) based on multi-skiff mode settings
 * and the layout of modules in the parent rack. This function is designed to be testable
 * in isolation by taking all necessary layout information as parameters.
 * @param children A constant reference to a vector of pointers to the parent widget's children (modules).
 * @param visibleChildrenBoundingBox The combined bounding box of all visible children in the parent widget.
 * @param opts A constant reference to a SkiffOpts object containing all drawing parameters.
 * @return A vector of Rect objects, each representing a skiff bounding box.
 */
std::vector<rack::math::Rect> CalculateSkiffBoxes(const std::list<rack::widget::Widget*>& children,
                                                  const rack::math::Rect& visibleChildrenBoundingBox,
                                                  const SkiffOpts& opts) {
  // Vector to store the final skiff bounding boxes.
  std::vector<rack::math::Rect> resultSkiffBoxes;

  // Case 1: Single Skiff Mode (multiSkiffMode is false)
  // In this mode, the skiff always covers the entire visible area occupied by modules.
  if (!opts.multiSkiffMode) {
    resultSkiffBoxes.push_back(visibleChildrenBoundingBox);
    return resultSkiffBoxes;
  }

  // Case 2: Multi-Skiff Mode
  // This mode involves potentially splitting the skiff into multiple boxes based on module layout.

  // Step A: Group modules by row and create initial horizontal segments within each row.
  // A map where the key is the row index (Y-position / grid height) and the value
  // is a vector of Rects representing module bounding boxes in that row.
  std::map<int, std::vector<rack::math::Rect>> rowsWithModuleBoxes;
  for (rack::widget::Widget* mw : children) {
    int moduleRow = static_cast<int>(mw->box.pos.y / rack::app::RACK_GRID_HEIGHT);
    rowsWithModuleBoxes[moduleRow].push_back(mw->box);
  }

  // Step B: Refine horizontal segments within each row based on horizontalSplitThreshold.
  // This process merges horizontally adjacent module boxes within the same row
  // if the gap between them is smaller than or equal to the defined threshold.
  std::map<int, std::vector<rack::math::Rect>> rowsWithMergedHorizontalSegments;
  for (auto& pair : rowsWithModuleBoxes) {
    std::vector<rack::math::Rect>& segmentsInRow = pair.second;
    if (segmentsInRow.empty()) continue;

    // Sort segments by their X position to process them left-to-right.
    std::sort(segmentsInRow.begin(), segmentsInRow.end(),
              [](const rack::math::Rect& a, const rack::math::Rect& b) { return a.pos.x < b.pos.x; });

    std::vector<rack::math::Rect> mergedSegments;
    // Start with the first segment in the row.
    mergedSegments.push_back(segmentsInRow[0]);

    // Iterate through the rest of the segments in the row.
    for (size_t i = 1; i < segmentsInRow.size(); ++i) {
      rack::math::Rect& currentSegment = segmentsInRow[i];
      rack::math::Rect& lastMergedSegment = mergedSegments.back();

      // Calculate the horizontal gap between the right edge of the last merged segment
      // and the left edge of the current segment.
      float gap = currentSegment.pos.x - lastMergedSegment.getRight();

      // If the gap is within the threshold (meaning modules are close enough),
      // expand the last merged segment to include the current one.
      if (gap <= opts.horizontalSplitThreshold * rack::app::RACK_GRID_WIDTH) {
        lastMergedSegment = lastMergedSegment.expand(currentSegment);
      } else {
        // If the gap is larger than the threshold, it signifies a distinct horizontal separation.
        // Finalize the current merged segment and start a new one with the current module.
        mergedSegments.push_back(currentSegment);
      }
    }
    // Store the horizontally merged segments for this row.
    rowsWithMergedHorizontalSegments[pair.first] = mergedSegments;
  }

  // Step C: Determine final skiff boxes based on the vertical splitting option.
  if (opts.splitVertically) {
    // If vertical splitting is enabled, each horizontally merged segment from each row
    // becomes its own distinct skiff box. This directly addresses the user's request
    // for splitting based on horizontal separation even when vertical splitting is active.
    for (const auto& pair : rowsWithMergedHorizontalSegments) {
      for (const auto& segment : pair.second) {
        resultSkiffBoxes.push_back(segment);
      }
    }
  } else {
    // If vertical splitting is NOT enabled, the goal is to have fewer, larger skiff boxes
    // that might span multiple rows, but still respect significant horizontal gaps.

    // If horizontal splitting is active (threshold > MIN), perform horizontal splitting
    // across the entire visible module area. This will create distinct horizontal segments
    // that span all relevant rows, effectively merging vertically where possible.
    if (static_cast<int>(opts.horizontalSplitThreshold) > static_cast<int>(HORIZONTAL_SPLIT_MIN)) {
      // Collect all individual module boxes from all rows into a single list.
      std::vector<rack::math::Rect> allModuleBoxes;
      for (const auto& pair : rowsWithModuleBoxes) {
        for (const auto& box : pair.second) {
          allModuleBoxes.push_back(box);
        }
      }

      if (allModuleBoxes.empty()) {
        // If no modules are present, return an empty list of skiff boxes.
        return resultSkiffBoxes;
      }

      // Sort all module boxes by their X position to process them left-to-right for horizontal merging.
      // If X positions are equal, sort by Y to maintain consistent ordering for vertical merging.
      std::sort(allModuleBoxes.begin(), allModuleBoxes.end(), [](const rack::math::Rect& a, const rack::math::Rect& b) {
        if (a.pos.x != b.pos.x) return a.pos.x < b.pos.x;
        return a.pos.y < b.pos.y;
      });

      // Start with the bounding box of the first module as the initial horizontal segment.
      rack::math::Rect currentHoriBox = allModuleBoxes[0];

      // Iterate through the rest of the module boxes.
      for (size_t i = 1; i < allModuleBoxes.size(); ++i) {
        rack::math::Rect& moduleBox = allModuleBoxes[i];
        // Calculate the horizontal gap between the current combined horizontal box and the next module.
        float gap = moduleBox.pos.x - currentHoriBox.getRight();

        // If the gap is within the threshold, expand the current horizontal box to include the module.
        // This merges modules horizontally across different rows if they are close enough.
        if (gap <= opts.horizontalSplitThreshold * rack::app::RACK_GRID_WIDTH) {
          currentHoriBox = currentHoriBox.expand(moduleBox);
        } else {
          // If the gap is larger, finalize the current horizontal box and add it to the results.
          // Then, start a new horizontal box with the current module.
          resultSkiffBoxes.push_back(currentHoriBox);
          currentHoriBox = moduleBox;
        }
      }
      // After the loop, add the last horizontal box to the results.
      resultSkiffBoxes.push_back(currentHoriBox);

    } else {
      // If horizontal splitting is not active (threshold is MIN),
      // and vertical splitting is also off, just use the overall bounding box of all visible modules.
      resultSkiffBoxes.push_back(visibleChildrenBoundingBox);
    }
  }
  return resultSkiffBoxes;
}

}  // namespace DANT
