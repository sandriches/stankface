#pragma once

#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "stankface/Filter.h"
#include "stankface/WavetableOscillator.h"

/** Draws one cycle of what the synth is currently producing.

    Not the raw wavetable: the cycle is run through the same oscillator and
    filter classes the voices use, so drive, cutoff and resonance reshape the
    curve exactly as they reshape the sound. Position and cutoff come from the
    engine's own modulated values rather than from the parameters, which means
    the drawing follows the LFO instead of showing a wave the synth is not
    playing.

    It builds the cycle itself rather than tapping the audio output. Tapping
    would mean a shared buffer and a trigger to line cycles up; running the same
    components on the UI thread costs a fraction of a percent of a core and
    cannot disturb the audio path at all, because none of the state is shared.

    It deliberately does not use Voice. Voice bundles the amp envelope, which a
    display wants held open, and takes a MIDI note where this needs a fixed
    frequency. The chain order is the only thing not shared, and it is one line.
*/
class WavetableDisplay : public juce::Component,
                         private juce::Timer
{
public:
    explicit WavetableDisplay(StankfaceAudioProcessor& processor);

    void paint(juce::Graphics& g) override;

private:
    /** Samples per drawn cycle.

        The display frequency is derived as sampleRate / this, so a cycle is a
        whole number of samples and the curve stays put instead of crawling
        sideways as the phase drifts. At common rates it lands in the low 40s of
        Hz, which is where this instrument is played anyway -- and the filter
        response the curve shows depends on pitch, so drawing at a bass
        frequency is the representative choice.
    */
    static constexpr int kCycleSamples = 1024;

    /** Cycles rendered per repaint, of which the last is drawn. The filter
        carries its state between repaints, so this is only about settling
        quickly after a knob moves rather than about reaching steady state. */
    static constexpr int kCyclesPerFrame = 4;

    static constexpr int kFramesPerSecond = 30;

    /** Fixed, so that drive and resonance pushing the level up is visible as
        the curve growing. Auto-scaling would normalise that away. */
    static constexpr float kVerticalRange = 1.3f;

    void timerCallback() override;
    void renderCycle();
    float parameterValue(stankface::ParamId id) const;

    StankfaceAudioProcessor& processor_;

    stankface::WavetableOscillator oscillator_;
    stankface::Filter filter_;

    std::vector<float> cycle_;
    double preparedSampleRate_ = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableDisplay)
};
