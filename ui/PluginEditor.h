#pragma once

#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PluginProcessor.h"
#include "StankfaceLookAndFeel.h"
#include "WavetableDisplay.h"

/** Control surface: a waveform display, then one labelled control per engine
    parameter.

    The controls are built straight from the engine's descriptor table rather
    than from a hand-written list, so adding a parameter to the engine puts a
    control here with no edit to this file. Stock JUCE widgets otherwise; the XY
    morph pad comes later.
*/
class StankfaceAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::ChangeListener
{
public:
    explicit StankfaceAudioProcessorEditor(StankfaceAudioProcessor& owner);
    ~StankfaceAudioProcessorEditor() override;

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

    /** One section of the panel: a heading and the controls under it, in the
        order the descriptor table lists them. */
    struct Group
    {
        juce::String name;
        juce::Label heading;
        std::vector<Control*> controls;
    };

    StankfaceAudioProcessor& processor_;

    // Declared before the components that use it, so it is still alive while
    // they are being destroyed.
    stankface_ui::StankfaceLookAndFeel lookAndFeel_;

    /** Holds every control at a fixed logical size, and is scaled as a whole to
        fill the window. Laying out once and scaling means resizing never
        reflows anything: there is one arrangement to reason about rather than
        one per window size. */
    juce::Component content_;

    int logicalWidth_ = 0;
    int logicalHeight_ = 0;

    WavetableDisplay display_;
    juce::MidiKeyboardComponent keyboard_;
    std::vector<std::unique_ptr<Control>> controls_;
    std::vector<std::unique_ptr<Group>> groups_;
    juce::Label title_;

    /** Factory preset selector: a list plus previous/next, so auditioning the
        set is one click per sound rather than open, pick, close. */
    juce::ComboBox presetBox_;
    juce::TextButton previousPreset_ { "<" };
    juce::TextButton nextPreset_ { ">" };

    Group& groupFor(const juce::String& name);

    /** Steps through the presets, wrapping at either end. */
    void stepPreset(int delta);

    /** Follows the processor when the program changes from anywhere,
        including the host's own preset menu. */
    void changeListenerCallback(juce::ChangeBroadcaster*) override;

    /** Places everything inside content_, once, at the logical size. */
    void layoutContent();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StankfaceAudioProcessorEditor)
};
