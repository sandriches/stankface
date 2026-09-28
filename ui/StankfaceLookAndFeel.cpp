#include "StankfaceLookAndFeel.h"

namespace stankface_ui {

StankfaceLookAndFeel::StankfaceLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, colours::background);

    setColour(juce::Label::textColourId, colours::textDim);

    setColour(juce::Slider::textBoxTextColourId, colours::text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxHighlightColourId, colours::accent.withAlpha(0.25f));

    setColour(juce::ComboBox::backgroundColourId, colours::panel);
    setColour(juce::ComboBox::textColourId, colours::text);
    setColour(juce::ComboBox::outlineColourId, colours::outline);
    setColour(juce::ComboBox::arrowColourId, colours::accent);

    setColour(juce::PopupMenu::backgroundColourId, colours::panel);
    setColour(juce::PopupMenu::textColourId, colours::text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId,
              colours::accent.withAlpha(0.20f));
    setColour(juce::PopupMenu::highlightedTextColourId, colours::text);

    setColour(juce::TextEditor::backgroundColourId, colours::panel);
    setColour(juce::TextEditor::textColourId, colours::text);
    setColour(juce::TextEditor::highlightColourId, colours::accent.withAlpha(0.25f));
    setColour(juce::TextEditor::focusedOutlineColourId, colours::accent);
}

void StankfaceLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                           int x, int y, int width, int height,
                                           float sliderPosProportional,
                                           float rotaryStartAngle,
                                           float rotaryEndAngle,
                                           juce::Slider& slider)
{
    const juce::Rectangle<float> bounds =
        juce::Rectangle<int>(x, y, width, height).toFloat().reduced(4.0f);

    const float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    if (radius <= 2.0f)
        return;

    const juce::Point<float> centre = bounds.getCentre();
    const float thickness = juce::jmax(2.5f, radius * 0.14f);
    const float arcRadius = radius - thickness * 0.5f;

    // Butt caps, not rounded: square ends are most of what stops this reading
    // as a stock rotary.
    const juce::PathStrokeType stroke(thickness,
                                      juce::PathStrokeType::curved,
                                      juce::PathStrokeType::butt);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                        rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(colours::track);
    g.strokePath(track, stroke);

    const float angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const bool bipolar = static_cast<bool>(
        slider.getProperties().getWithDefault(kBipolarProperty, false));

    const float midAngle = (rotaryStartAngle + rotaryEndAngle) * 0.5f;
    const float originAngle = bipolar ? midAngle : rotaryStartAngle;

    if (std::abs(angle - originAngle) > 1.0e-3f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                            juce::jmin(originAngle, angle),
                            juce::jmax(originAngle, angle), true);

        g.setColour(slider.isEnabled() ? colours::accent : colours::accentDim);
        g.strokePath(value, stroke);
    }

    // A single hard pointer rather than a filled cap or a dot.
    const juce::Point<float> from =
        centre.getPointOnCircumference(arcRadius * 0.32f, angle);
    const juce::Point<float> to =
        centre.getPointOnCircumference(arcRadius - thickness * 0.95f, angle);

    g.setColour(colours::text);
    g.drawLine({ from, to }, 2.0f);

    // A tick at the centre of a bipolar control's travel, so zero is findable
    // without reading the number.
    if (bipolar)
    {
        const float inner = arcRadius + thickness * 0.5f + 1.5f;
        g.setColour(colours::textDim);
        g.drawLine({ centre.getPointOnCircumference(inner, midAngle),
                     centre.getPointOnCircumference(inner + 3.0f, midAngle) },
                   1.0f);
    }
}

void StankfaceLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height,
                                        bool, int, int, int, int,
                                        juce::ComboBox& box)
{
    const juce::Rectangle<float> bounds =
        juce::Rectangle<int>(0, 0, width, height).toFloat().reduced(0.5f);

    g.setColour(box.findColour(juce::ComboBox::backgroundColourId));
    g.fillRoundedRectangle(bounds, 2.0f);

    g.setColour(box.hasKeyboardFocus(true) ? colours::accent : colours::outline);
    g.drawRoundedRectangle(bounds, 2.0f, 1.0f);

    // Drawn as a stroked chevron rather than a filled triangle so it stays thin
    // at any size, which matches the pointer on the rotaries.
    const float cx = bounds.getRight() - 11.0f;
    const float cy = bounds.getCentreY();

    juce::Path chevron;
    chevron.startNewSubPath(cx - 3.5f, cy - 2.0f);
    chevron.lineTo(cx, cy + 2.2f);
    chevron.lineTo(cx + 3.5f, cy - 2.0f);

    g.setColour(box.findColour(juce::ComboBox::arrowColourId));
    g.strokePath(chevron, juce::PathStrokeType(1.5f));
}

} // namespace stankface_ui
