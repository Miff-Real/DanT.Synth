# DanT.Synth Layout System

Creating Eurorack modules programmatically requires precise mapping of UI layouts from abstract units to absolute screen
pixel coordinates. In VCV Rack `libRack`, developers often think in millimeters and convert to vectors, but DanT.Synth
uses a custom layout scaling grid provided centrally via `src/plugin.hpp` based directly on `HP` and 32 vertical rows.

## The Grid Abstractions

The `DANT::layout(float column, float row)` system returns absolute UI coordinates (`rack::math::Vec`) where:

1. **Columns** (X-axis) correspond closely to half-HP divisions across the module.
   * `RACK_GRID_WIDTH` is `15.0f` pixels, representing 1 HP width.
   * Total width of an 8 HP module, for instance, spans strictly from 0 to 120 pixels.
   * The `column` variable scales intuitively: Column `4.5` perfectly maps to the exact horizontal center of an 8 HP
     module (60px).
   * A clean standard distribution for 3 horizontal items usually sits on `2.0`, `4.5`, and `7.0`.

2. **Rows** (Y-axis) distribute components vertically via an internal constant step ratio over the total 1U rack height
   (usually 380px).
   * There are `32` pseudo-rows dividing the panel's vertical space (`RACK_GRID_HEIGHT / 32.0f`).
   * Row intervals of `1.0` equal approximately a vertical step height of `~23.75` pixels.

## Perfect Cross Spacing Algorithm

As discovered through empirical design mapping during the Bend module iteration: **A typical knob, port, or CKSS switch
can be vertically grouped or arrayed efficiently on a `2-by-1` relative spacing rule**.

Whenever mapping 4 clustered elements (e.g. cross or diamond pattern of switch combinations around a central X/Y pair):

* **Left vs Right Spacing**: Use increments of `+/- 2.0` layout x-units.
* **Top vs Bottom Spacing**: Use increments of `+/- 1.0` layout y-units.

Example placement of a 4-point cross around a central `(4.5, X)` anchor:
* Top:    `layout(4.5, 6.0)`
* Right:  `layout(6.5, 8.0)`
* Left:   `layout(2.5, 8.0)`
* Bottom: `layout(4.5, 10.0)`

Notice how Left/Right span `2.0` columns from the center, accommodating wide ports/switches horizontally, whereas
vertically the rows increment `1.0` steps per logical grouping!

## Text Labeling Margin Guidelines

Because `DANT::layout()` coordinates assign the absolute center point of components (via `createInputCentered`), placing
text strings like "Toward/From" directly around CKSS switches requires precise sub-unit fractional nudging on the
Y-axis.

When placing dual-function textual labels visually representing the two states of a `CKSS` Switch, follow this strict
vertical relative grouping:

* **Switch origin**: Place the CKSS switch at its target `Y` coordinate (e.g. `6.7f`).
* **Text Above Switch (Top State)**: Nudge Y-axis by `-0.7` units from the switch's center `y` (e.g. `6.0f`).
* **Text Below Switch (Bottom State)**: Nudge Y-axis by `+0.7` units from the switch's center `y` (e.g. `7.4f`).
* **Underlying CV Port**: Place the CV input port at `+1.6` Y-units from the switch's center `y` (e.g. `8.3f`).

These exact margins ensure the standard UI font size (`opts.size = 9.5f`) sits comfortably floating around the switch
without colliding with the toggle switch graphics, touching the input port bezel, or looking misaligned.

## Material Symbols Rendering

When utilizing custom icons from embedded FontAwesome or Material Symbols fonts (e.g. `MaterialSymbolsSharp`), you must
locate the correct target unicode representation using the `.codepoints` dictionary.

To guarantee the symbol strings are UTF-8 encoded into memory flawlessly by the compiler and successfully passed into
`DANT::Fonts::drawSymbols()`, it is highly advised to initialize the unicode string natively inside the `cpp` file or
shared `hpp` state via static constants:
```cpp
// Correct
static const std::string BEND_DIR_UP{"\ue5d8"};
DANT::Fonts::drawSymbols(args, BEND_DIR_UP, opts);
```
Passing short inline strings like `DANT::Fonts::drawSymbols(args, "\ue5d8", opts);` inline during the `draw()` loop can
lead to broken encoding mappings resulting in missing box-characters or compilation warnings depending on the compiler.

## Custom Context Menu Sliders

When building numerical sliders for right-click context menus, Rack's default `rack::Quantity` instances strictly bind
the string representation to the internal stored float value. When you need a slider to natively display advanced
human-readable scalings without relying on external static labels (e.g. formatting a `0.10f` internal multiplier into a
clean "10%" integer display), utilize the custom `DANT::FloatValueQuantity` wrapper in `src/shared/menu-slider.hpp`.

Instead of placing loose `rack::createMenuLabel` headers above sliders, construct the slider directly with formatting:

```cpp
auto* durSlider = new DANT::MenuSlider(
    new DANT::FloatValueQuantity(
        "Unbend Duration",           // Name
        0.0f,                        // Min value
        1.0f,                        // Max value
        0.10f,                       // Default value
        &module->unbendDurationPct,  // Target float pointer
        "%",                         // Suffix unit
        100.0f,                      // Display scale multiplier (Display = Internal * 100)
        "%.0f"                       // C-style string format (no decimals)
    ),
    DANT::RGB_SLIDER_WIDTH
);
menu->addChild(durSlider);
```

This ensures the user's interface remains completely uncluttered by redundant title labels, handles mathematical
scalings automagically under the hood (such as V/Oct conversions to specific integer Cents variants), and seamlessly
pushes and pulls updates back to the core module state variables.

## DSP Parameter Extraction Pattern

To maintain a clean and highly readable `process()` block within modules, complex parameter pooling and CV modulation
calculations should be extracted into `inline` helper methods defined just above `process()`.

Instead of polluting the main DSP loop with inline voltage reading, clamping, and boolean mapping, e.g.:
```cpp
// Bad
float amountCV = inputs[BEND_AMOUNT_CV_INPUT].getNormalPolyVoltage(0.0f, c);
float amountSemitones = params[BEND_AMOUNT_PARAM].getValue() + (amountCV * 12.0f);
amountSemitones = std::max(1.0f, amountSemitones); 
float amountVolts = amountSemitones / 12.0f;
```

Extract the calculation logic so the main processing loop relies purely on explicitly named functional calls
representing the final usable state:
```cpp
// Correct
inline float readBendAmount(int channel) {
  float amountCV = inputs[BEND_AMOUNT_CV_INPUT].getNormalPolyVoltage(0.0f, channel);
  float amountSemitones = params[BEND_AMOUNT_PARAM].getValue() + (amountCV * 12.0f);
  return std::max(1.0f, amountSemitones);
}

void process(const rack::engine::Module::ProcessArgs& args) override {
  // ...
  float amountVolts = readBendAmount(c) / 12.0f;
}
```

By heavily utilizing `inline`, we ensure these helpers carry zero function-call overhead and are directly embedded into
the DSP loop exactly as they were written, optimizing both readability and runtime performance simultaneously.

## Rack DSP Math Optimizations

When processing signals within the core `process()` audio thread loop, avoid standard heavy scalar math libraries where
possible.

* **Exponents:** Never use `std::pow(2.0f, x)`. Rack provides highly-optimized Taylor series approximations. Use
  `rack::dsp::exp2_taylor5(x)` for scalar floats or `rack::simd::float_4` arrays. It is significantly faster and
  maintains sufficient precision for audio scaling.
* **Bounds Control:** Unlike complex powers or exponents, standard compiler boundaries like `std::min()`, `std::max()`,
  `std::abs()`, and `rack::math::clamp()` are perfectly safe and optimal to use. They naturally compile down to
  branchless bounds-limit instructions. Do not build elaborate substitutes for these.

## Widget Drawing Loops (Polyphony State)

When drawing arrays of graphical elements inside a `Widget::draw()` loop (e.g., rendering a GridLight for 16 poly
channels), it is strictly required to initialize all temporary graphical states (like `NVGColor`) *inside* the array
loop.

If a color state is declared outside the loop and modified by a live channel (e.g., Ch 1 is bright green), an unused
subsequent channel (e.g., Ch 2 is `0.0f` and therefore skips evaluation logic) will mistakenly render using the previous
channel's color state! Always reset the "paintbrush" explicitly for every element drawn.
