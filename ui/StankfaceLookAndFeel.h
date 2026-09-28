#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace stankface_ui {

/** Near-black with one acid accent.

    A single accent colour for the knobs and the waveform trace both, so the
    panel reads as one instrument rather than a grid of unrelated widgets.
*/
namespace colours {

const juce::Colour background { 0xff0d0f13 };
const juce::Colour panel      { 0xff14171e };
const juce::Colour outline    { 0xff23272f };
const juce::Colour track      { 0xff23272f };
const juce::Colour accent     { 0xffb4ff3a };
const juce::Colour accentDim  { 0xff4e6a1c };
const juce::Colour text       { 0xffe6eaee };
const juce::Colour textDim    { 0xff79818d };

} // namespace colours

/** Set on a slider to say its value grows from the centre of its travel rather
    than from the minimum, which is how a bipolar control should read. */
inline const juce::Identifier kBipolarProperty { "stankfaceBipolar" };

/** Flat, hard-edged rotaries: an arc and a pointer, no bevels, gradients or
    shadows. Square stroke caps rather than rounded ones do most of the work --
    rounded caps are what make a stock rotary look soft.
*/
class StankfaceLookAndFeel : public juce::LookAndFeel_V4
{
public:
    StankfaceLookAndFeel();

    void drawRotarySlider(juce::Graphics& g,
                          int x, int y, int width, int height,
                          float sliderPosProportional,
                          float rotaryStartAngle,
                          float rotaryEndAngle,
                          juce::Slider& slider) override;

    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StankfaceLookAndFeel)
};

} // namespace stankface_ui
