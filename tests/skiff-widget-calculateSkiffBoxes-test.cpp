#include <algorithm>  // std::sort
#include <memory>     // std::unique_ptr
#include <rack.hpp>
#include <string>
#include <vector>

#include "../src/shared/skiffs.hpp"
#include "catch2/catch.hpp"

// Helper function to compare two vectors of Rects
bool compareRectVectors(const std::vector<rack::math::Rect>& a, const std::vector<rack::math::Rect>& b) {
  if (a.size() != b.size()) {
    return false;
  }
  // Sort both vectors to ensure consistent order for comparison
  std::vector<rack::math::Rect> sortedA = a;
  std::vector<rack::math::Rect> sortedB = b;
  std::sort(sortedA.begin(), sortedA.end(), [](const rack::math::Rect& r1, const rack::math::Rect& r2) {
    if (r1.pos.y != r2.pos.y) return r1.pos.y < r2.pos.y;
    return r1.pos.x < r2.pos.x;
  });
  std::sort(sortedB.begin(), sortedB.end(), [](const rack::math::Rect& r1, const rack::math::Rect& r2) {
    if (r1.pos.y != r2.pos.y) return r1.pos.y < r2.pos.y;
    return r1.pos.x < r2.pos.x;
  });

  for (size_t i = 0; i < sortedA.size(); ++i) {
    // Use a small tolerance for floating point comparisons if necessary,
    // but for Rects, direct comparison is usually fine for exact matches.
    if (sortedA[i].pos.x != sortedB[i].pos.x || sortedA[i].pos.y != sortedB[i].pos.y ||
        sortedA[i].size.x != sortedB[i].size.x || sortedA[i].size.y != sortedB[i].size.y) {
      return false;
    }
  }
  return true;
}

// Mock Widget to simulate a module for testing purposes.
// It explicitly sets its parent to nullptr on destruction to avoid VCV Rack's Widget assertion.
struct MockWidget : rack::widget::Widget {
  // Constructor to set position and size
  MockWidget(float x, float y, float w, float h) {
    box.pos = rack::math::Vec(x, y);
    box.size = rack::math::Vec(w, h);
  }

  // Destructor to clear the parent pointer
  ~MockWidget() override {
    parent = nullptr;  // Crucial to prevent "Assertion failed: (!parent)"
  }
};

TEST_CASE("DANT::CalculateSkiffBoxes") {  // Test case name updated to reflect namespace
  // Define a standard module size for consistent testing
  const float MODULE_HP_WIDTH = rack::app::RACK_GRID_WIDTH;      // 1 HP
  const float MODULE_UNIT_HEIGHT = rack::app::RACK_GRID_HEIGHT;  // 1U

  SECTION("Single Skiff Mode - multiSkiffMode = false") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = false;  // Explicitly set for clarity

    // Create mock modules using unique_ptr for proper memory management
    std::vector<std::unique_ptr<MockWidget>> ownedChildren;
    ownedChildren.push_back(std::unique_ptr<MockWidget>(
        new MockWidget(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 4 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT)));
    ownedChildren.push_back(std::unique_ptr<MockWidget>(
        new MockWidget(5 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 2 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT)));
    ownedChildren.push_back(std::unique_ptr<MockWidget>(
        new MockWidget(2 * MODULE_HP_WIDTH, 4 * MODULE_UNIT_HEIGHT, 3 * MODULE_HP_WIDTH, 2 * MODULE_UNIT_HEIGHT)));

    // Create a vector of raw pointers to pass to the function
    std::list<rack::widget::Widget*> childrenPointers;
    for (const auto& child : ownedChildren) {
      childrenPointers.push_back(child.get());
    }

    // The bounding box for these modules
    rack::math::Rect expectedBoundingBox = rack::math::Rect(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT,
                                                            (5 + 2) * MODULE_HP_WIDTH, (4 + 2) * MODULE_UNIT_HEIGHT);
    // Manually expand to ensure it truly encompasses all
    for (const auto& child : ownedChildren) {
      expectedBoundingBox = expectedBoundingBox.expand(child->box);
    }

    // Call the standalone function directly
    std::vector<rack::math::Rect> result =
        DANT::CalculateSkiffBoxes(childrenPointers,
                                  expectedBoundingBox,  // Pass the pre-calculated bounding box
                                  opts);

    REQUIRE(result.size() == 1);
    CHECK(compareRectVectors(result, {expectedBoundingBox}));
  }

  SECTION("Multi Skiff Mode - Vertical Split True, Horizontal Threshold 1 HP (Split within rows)") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = true;
    opts.splitVertically = true;
    opts.horizontalSplitThreshold = 1.0f;  // 1 HP gap

    // Rack Layout:
    // Row 0: [M][M] ( )[M]   (gap 1HP)
    // Row 1: [M]   ( )[M]   (gap 2HP)
    //        (vertical gap)
    // Row 3: [M][M][M]
    // Row 4:   ( )[M]        (gap 1HP)

    std::vector<std::unique_ptr<MockWidget>> ownedChildren;
    // Row 0
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m1: [M][M]
    ownedChildren.push_back(
        std::unique_ptr<MockWidget>(new MockWidget(3 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                                                   1 * MODULE_UNIT_HEIGHT)));  // m2: [M] (gap 1HP from m1)
    // Row 1
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m3: [M]
    ownedChildren.push_back(
        std::unique_ptr<MockWidget>(new MockWidget(3 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                                                   1 * MODULE_UNIT_HEIGHT)));  // m4: [M] (gap 2HP from m3)
    // Row 3 (vertical gap from Row 1)
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT, 3 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m5: [M][M][M]
    // Row 4
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        1 * MODULE_HP_WIDTH, 4 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
        1 * MODULE_UNIT_HEIGHT)));  // m6: [M] (gap 1HP from m5's right edge if it were there, but it's a new row)
    ownedChildren.push_back(
        std::unique_ptr<MockWidget>(new MockWidget(3 * MODULE_HP_WIDTH, 4 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                                                   1 * MODULE_UNIT_HEIGHT)));  // m7: [M] (gap 1HP from m6)

    std::list<rack::widget::Widget*> childrenPointers;
    for (const auto& child : ownedChildren) {
      childrenPointers.push_back(child.get());
    }

    rack::math::Rect overallBoundingBox = rack::math::Rect();
    if (!ownedChildren.empty()) {
      overallBoundingBox = ownedChildren[0]->box;
      for (size_t i = 1; i < ownedChildren.size(); ++i) {
        overallBoundingBox = overallBoundingBox.expand(ownedChildren[i]->box);
      }
    }

    std::vector<rack::math::Rect> result = DANT::CalculateSkiffBoxes(childrenPointers, overallBoundingBox, opts);

    // Expected skiff boxes based on the logic (gap <= threshold for merge, splitVertically = true):
    // Row 0: m1 (0,0,2,1) and m2 (3,0,1,1). Gap = 1HP. Threshold = 1HP. 1 <= 1 is true. MERGE. -> (0,0,4,1)
    // Row 1: m3 (0,1,1,1) and m4 (3,1,1,1). Gap = 2HP. Threshold = 1HP. 2 <= 1 is false. SPLIT. -> (0,1,1,1), (3,1,1,1)
    // Row 3: m5 (0,3,3,1). No horizontal splits within its row. -> (0,3,3,1)
    // Row 4: m6 (1,4,1,1) and m7 (3,4,1,1). Gap = 1HP. Threshold = 1HP. 1 <= 1 is true. MERGE. -> (1,4,3,1)
    // Total expected: 1 (R0) + 2 (R1) + 1 (R3) + 1 (R4) = 5 boxes.
    std::vector<rack::math::Rect> expectedBoxes = {
        rack::math::Rect(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 4 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // m1+m2
        rack::math::Rect(0 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // m3
        rack::math::Rect(3 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // m4
        rack::math::Rect(0 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT, 3 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // m5
        rack::math::Rect(1 * MODULE_HP_WIDTH, 4 * MODULE_UNIT_HEIGHT, 3 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT)  // m6+m7
    };

    REQUIRE(result.size() == expectedBoxes.size());
    CHECK(compareRectVectors(result, expectedBoxes));
  }

  SECTION("Multi Skiff Mode - Horizontal Split True, Vertical Split False, Threshold 2 HP (Merge across rows)") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = true;
    opts.splitVertically = false;          // Important for this test
    opts.horizontalSplitThreshold = 2.0f;  // 2 HP gap

    // Rack Layout:
    // [M][M]( )( )[M][M][M] (m1, m2)
    // [M]   ( )   [M]       (m3, m4)

    std::vector<std::unique_ptr<MockWidget>> ownedChildren;
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m1
    ownedChildren.push_back(
        std::unique_ptr<MockWidget>(new MockWidget(4 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 3 * MODULE_HP_WIDTH,
                                                   1 * MODULE_UNIT_HEIGHT)));  // m2 (2 HP gap from m1)
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m3
    ownedChildren.push_back(
        std::unique_ptr<MockWidget>(new MockWidget(4 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                                                   1 * MODULE_UNIT_HEIGHT)));  // m4 (3 HP gap from m3)

    std::list<rack::widget::Widget*> childrenPointers;
    for (const auto& child : ownedChildren) {
      childrenPointers.push_back(child.get());
    }

    rack::math::Rect overallBoundingBox = rack::math::Rect();
    if (!ownedChildren.empty()) {
      overallBoundingBox = ownedChildren[0]->box;
      for (size_t i = 1; i < ownedChildren.size(); ++i) {
        overallBoundingBox = overallBoundingBox.expand(ownedChildren[i]->box);
      }
    }

    std::vector<rack::math::Rect> result = DANT::CalculateSkiffBoxes(childrenPointers, overallBoundingBox, opts);

    // Expected: one large horizontal box spanning both rows
    // Modules: m1(0,0,2,1), m2(4,0,3,1), m3(0,1,1,1), m4(4,1,1,1)
    // Sorted: m1, m3, m2, m4
    // Start with currentHoriBox = m1 (0,0,2,1)
    // Process m3 (0,1,1,1): gap = 0 - 2 = -2. Expand. currentHoriBox = (0,0,2,2) (covers m1, m3)
    // Process m2 (4,0,3,1): gap = 4 - 2 = 2HP. Threshold = 2HP. 2 <= 2 is true. MERGE.
    //    currentHoriBox becomes (0,0,7,2) (covers m1, m3, m2)
    // Process m4 (4,1,1,1): gap = 4 - 7 = -3. Expand. currentHoriBox remains (0,0,7,2) (covers m1, m3, m2, m4)
    // Final expected: (0,0,7,2)
    std::vector<rack::math::Rect> expectedBoxes = {
        rack::math::Rect(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 7 * MODULE_HP_WIDTH, 2 * MODULE_UNIT_HEIGHT)};

    REQUIRE(result.size() == expectedBoxes.size());
    CHECK(compareRectVectors(result, expectedBoxes));
  }

  SECTION("Multi Skiff Mode - Vertical Split False, Horizontal Threshold Large (No Horizontal Split)") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = true;
    opts.splitVertically = false;
    opts.horizontalSplitThreshold = 100.0f;  // Very large threshold, should always merge horizontally

    // Rack Layout:
    // [M]      [M]
    //    [M]

    std::vector<std::unique_ptr<MockWidget>> ownedChildren;
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m1
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        5 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m2
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m3

    std::list<rack::widget::Widget*> childrenPointers;
    for (const auto& child : ownedChildren) {
      childrenPointers.push_back(child.get());
    }

    rack::math::Rect overallBoundingBox = rack::math::Rect();
    if (!ownedChildren.empty()) {
      overallBoundingBox = ownedChildren[0]->box;
      for (size_t i = 1; i < ownedChildren.size(); ++i) {
        overallBoundingBox = overallBoundingBox.expand(ownedChildren[i]->box);
      }
    }

    std::vector<rack::math::Rect> result = DANT::CalculateSkiffBoxes(childrenPointers, overallBoundingBox, opts);

    // Expected: One single large box encompassing all modules, as threshold is very large.
    std::vector<rack::math::Rect> expectedBoxes = {
        rack::math::Rect(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 6 * MODULE_HP_WIDTH,
                         2 * MODULE_UNIT_HEIGHT)  // From (0,0) to (5+1, 1+1)
    };

    REQUIRE(result.size() == expectedBoxes.size());
    CHECK(compareRectVectors(result, expectedBoxes));
  }

  SECTION("Multi Skiff Mode - Vertical Split True, Horizontal Threshold Large (No Horizontal Split within Rows)") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = true;
    opts.splitVertically = true;
    opts.horizontalSplitThreshold = 100.0f;  // Very large threshold, should always merge horizontally within rows

    // Rack Layout:
    // Row 0: [M][M] [M]
    // Row 1: [M] [M][M]
    //        (gap)
    // Row 3: [M]

    std::vector<std::unique_ptr<MockWidget>> ownedChildren;
    // Row 0
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m1
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        3 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m2 (1 HP gap)
    // Row 1
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m3
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 2 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m4 (1 HP gap)
    // Row 3
    ownedChildren.push_back(std::unique_ptr<MockWidget>(new MockWidget(
        0 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT)));  // m5

    std::list<rack::widget::Widget*> childrenPointers;
    for (const auto& child : ownedChildren) {
      childrenPointers.push_back(child.get());
    }

    rack::math::Rect overallBoundingBox = rack::math::Rect();
    if (!ownedChildren.empty()) {
      overallBoundingBox = ownedChildren[0]->box;
      for (size_t i = 1; i < ownedChildren.size(); ++i) {
        overallBoundingBox = overallBoundingBox.expand(ownedChildren[i]->box);
      }
    }

    std::vector<rack::math::Rect> result = DANT::CalculateSkiffBoxes(childrenPointers, overallBoundingBox, opts);

    // Expected: Vertical splits occur, but within each row, all modules merge horizontally
    // Row 0: m1(0,0,2,1) and m2(3,0,1,1). Gap 1HP. Threshold 100HP. MERGE. -> (0,0,4,1)
    // Row 1: m3(0,1,1,1) and m4(2,1,2,1). Gap 1HP. Threshold 100HP. MERGE. -> (0,1,4,1)
    // Row 3: m5(0,3,1,1). No horizontal splits. -> (0,3,1,1)
    std::vector<rack::math::Rect> expectedBoxes = {
        rack::math::Rect(0 * MODULE_HP_WIDTH, 0 * MODULE_UNIT_HEIGHT, 4 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // Merged Row 0
        rack::math::Rect(0 * MODULE_HP_WIDTH, 1 * MODULE_UNIT_HEIGHT, 4 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT),  // Merged Row 1
        rack::math::Rect(0 * MODULE_HP_WIDTH, 3 * MODULE_UNIT_HEIGHT, 1 * MODULE_HP_WIDTH,
                         1 * MODULE_UNIT_HEIGHT)  // Row 3
    };

    REQUIRE(result.size() == expectedBoxes.size());
    CHECK(compareRectVectors(result, expectedBoxes));
  }

  SECTION("Multi Skiff Mode - No Modules") {
    DANT::SkiffOpts opts;
    opts.multiSkiffMode = true;
    opts.splitVertically = true;  // Test both cases
    opts.horizontalSplitThreshold = 1.0f;

    std::vector<std::unique_ptr<MockWidget>> ownedChildren;              // Empty
    std::list<rack::widget::Widget*> childrenPointers;                   // Empty
    rack::math::Rect overallBoundingBox = rack::math::Rect(0, 0, 0, 0);  // Empty bounding box

    std::vector<rack::math::Rect> result = DANT::CalculateSkiffBoxes(childrenPointers, overallBoundingBox, opts);
    REQUIRE(result.empty());  // Should return an empty vector
  }
}
