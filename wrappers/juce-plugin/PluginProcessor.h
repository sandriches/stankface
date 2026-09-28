#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "stankface/Params.h"
#include "stankface/WavetableEngine.h"

/** True for parameters the host and editor should present as a list of named
    options rather than a continuous control. */
bool isChoiceParameter(stankface::ParamId id);

/** Writes a natural value the way it should appear on screen.

    Shared by the host's automation display and the editor's own readouts, so a
    value cannot read one way in the plugin window and another in an automation
    lane. The rules themselves come from the engine's descriptor table. */
juce::String formatParamValue(stankface::ParamId id, float naturalValue);

/** The inverse, for typing a value into a control. */
float parseParamValue(stankface::ParamId id, const juce::String& text);

/** JUCE wrapper around the engine.

    Deliberately thin. Everything here is host plumbing -- parameter objects,
    MIDI decoding, buffer layout, state save/load -- and none of it reaches into
    the DSP. The engine is linked as a plain static library and driven through
    its five public methods, which is what keeps it usable from anything else.
*/
class StankfaceAudioProcessor : public juce::AudioProcessor
{
public:
    StankfaceAudioProcessor();
    ~StankfaceAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int maximumExpectedSamplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }

    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override;

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState& parameters() { return parameters_; }

    /** The engine's modulation state, made safe for the editor to read.

        Published once per block rather than once per sample: the editor repaints
        at a few tens of frames a second, so a value one block old is not
        something anyone can see, and relaxed atomics keep the audio thread free
        of locks. */
    stankface::WavetableEngine::DisplayState displaySnapshot() const;

    /** Shared with the on-screen keyboard.

        Notes played on it are folded into the incoming MIDI buffer each block,
        so they reach the engine by exactly the same path as notes from the host
        and need no separate handling. The traffic goes both ways: the state is
        also updated from incoming MIDI, so the keys light up when the host plays
        rather than only when they are clicked. */
    juce::MidiKeyboardState& keyboardState() { return keyboardState_; }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void pushParametersToEngine();
    void handleMidiMessage(const juce::MidiMessage& message);

    juce::AudioProcessorValueTreeState parameters_;

    // Raw atomics rather than parameter lookups: this is read once per block on
    // the audio thread, and getParameter-style lookups are not worth doing there.
    std::atomic<float>* paramValues_[stankface::kNumParams] = {};

    stankface::WavetableEngine engine_;

    juce::MidiKeyboardState keyboardState_;

    // Written on the audio thread, read by the editor. Relaxed ordering is
    // enough: these two are independent readings for drawing, not a pair that
    // has to agree with each other or with anything else.
    std::atomic<float> displayPosition_ { 0.0f };
    std::atomic<float> displayCutoff_ { 1000.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StankfaceAudioProcessor)
};
