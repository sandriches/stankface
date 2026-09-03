#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"

/** Minimal control surface: one labelled control per engine parameter.

    Built straight from the engine's descriptor table rather than from a
    hand-written list, so adding a parameter to the engine puts a control here
    with no edit to this file. Stock JUCE widgets and no custom graphics -- the
    XY morph pad and wavetable display come later.
*/
class StankfaceAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit StankfaceAudioProcessorEditor(StankfaceAudioProcessor& owner);
    ~StankfaceAudioProcessorEditor() override = default;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    /** One parameter's label plus whichever control suits its type. */
    struct Control
    {
        juce::Label label;
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::ComboBox> comboBox;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sliderAttachment;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> comboAttachment;
    };

    StankfaceAudioProcessor& processor_;
    std::vector<std::unique_ptr<Control>> controls_;
    juce::Label title_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StankfaceAudioProcessorEditor)
};
