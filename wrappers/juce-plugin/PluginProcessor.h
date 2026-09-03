#pragma once

#include <atomic>

#include <juce_audio_processors/juce_audio_processors.h>

#include "stankface/Params.h"
#include "stankface/WavetableEngine.h"

/** True for parameters the host and editor should present as a list of named
    options rather than a continuous control. */
bool isChoiceParameter(stankface::ParamId id);

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

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    void pushParametersToEngine();
    void handleMidiMessage(const juce::MidiMessage& message);

    juce::AudioProcessorValueTreeState parameters_;

    // Raw atomics rather than parameter lookups: this is read once per block on
    // the audio thread, and getParameter-style lookups are not worth doing there.
    std::atomic<float>* paramValues_[stankface::kNumParams] = {};

    stankface::WavetableEngine engine_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StankfaceAudioProcessor)
};
