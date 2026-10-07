#pragma once

#include <array>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "stankface/Filter.h"
#include "stankface/WavetableOscillator.h"

/** Draws the whole wavetable as a 3D terrain, with the cycle the synth is
    playing right now lit up inside it.

    The terrain is a stack of slices across the morph, back to front, drawn with
    hidden lines so it reads as a surface rather than a tangle. Every slice is
    run through its own oscillator and filter, the same classes the voices use,
    so drive, cutoff and resonance reshape the whole landscape exactly as they
    reshape the sound -- close the filter and the terrain flattens.

    The live cycle is drawn over the terrain at the depth of the current
    position, with a glow and a short phosphor trail behind it. Position and
    cutoff for it come from the engine's own modulated values rather than from
    the parameters, so the bright trace rides the LFO through the stack instead
    of sitting where the knob is.

    It builds the cycles itself rather than tapping the audio output. Tapping
    would mean a shared buffer and a trigger to line cycles up; running the same
    components on the UI thread cannot disturb the audio path at all, because
    none of the state is shared.

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

    /** Cycles rendered per repaint for the live trace, of which the last is
        drawn. The filter carries its state between repaints, so this is only
        about settling quickly after a knob moves rather than about reaching
        steady state. */
    static constexpr int kCyclesPerFrame = 4;

    static constexpr int kFramesPerSecond = 30;

    /** Fixed, so that drive and resonance pushing the level up is visible as
        the curve growing. Auto-scaling would normalise that away. */
    static constexpr float kVerticalRange = 1.3f;

    /** Slices in the terrain. Dense enough to read as a surface; each one is a
        whole oscillator and filter, so this is the number that sets the cost. */
    static constexpr int kNumSlices = 24;

    /** Points per terrain slice. The slices are background and are drawn
        small, so they are box-filtered down from the full cycle. */
    static constexpr int kSlicePoints = 256;

    /** Points in the live trace. About one per physical pixel of the front
        slice on a high-density screen; the full cycle would be twice that,
        and every point is paid for again in each glow stroke. */
    static constexpr int kLivePoints = 512;

    /** Past live cycles kept for the phosphor trail. */
    static constexpr int kTrailLength = 10;

    /** Frames the terrain keeps rendering after its inputs stop changing, to
        let the filters ring out. Once that runs out the slices are periodic
        and identical frame to frame, so rendering them again would be wasted:
        a static patch costs nothing for the terrain. */
    static constexpr int kSettleFrames = 30;

    /** While the LFO is sweeping the cutoff the terrain changes every frame,
        and redrawing two dozen slices that often costs several times what the
        rest of the display does. It is redrawn on every this-many frames
        instead; the live trace stays at the full rate, so it is the background
        that steps and the part the eye follows that stays smooth. */
    static constexpr int kSweepingTerrainInterval = 3;

    struct Slice
    {
        stankface::WavetableOscillator oscillator;
        stankface::Filter filter;
        float position = 0.0f;
        std::array<float, kSlicePoints> points {};
    };

    struct TrailCycle
    {
        float position = 0.0f;
        std::array<float, kSlicePoints> points {};
    };

    /** Everything the terrain depends on. When none of it moves the terrain
        is left alone. */
    struct TerrainInputs
    {
        int table = -1;
        float resonance = 0.0f;
        float drive = 0.0f;
        float cutoff = 0.0f;

        // Exact on purpose: any change at all, however small, has to be
        // allowed to settle into the terrain.
        bool operator!=(const TerrainInputs& other) const
        {
            return table != other.table
                || ! juce::exactlyEqual(resonance, other.resonance)
                || ! juce::exactlyEqual(drive, other.drive)
                || ! juce::exactlyEqual(cutoff, other.cutoff);
        }
    };

    void timerCallback() override;
    void prepare(double sampleRate);
    void renderCycle();
    void renderTerrain(const TerrainInputs& inputs);
    void paintTerrain(juce::Graphics& g) const;
    float parameterValue(stankface::ParamId id) const;

    StankfaceAudioProcessor& processor_;

    stankface::WavetableOscillator oscillator_;
    stankface::Filter filter_;

    std::vector<float> cycle_;
    std::array<float, kLivePoints> livePoints_ {};
    float livePosition_ = 0.0f;
    double preparedSampleRate_ = 0.0;

    std::array<Slice, kNumSlices> slices_;
    std::vector<float> terrainScratch_;
    TerrainInputs terrainInputs_;
    int settleFramesLeft_ = 0;
    int framesSinceTerrain_ = 0;

    /** The terrain, rasterised once at the screen's real pixel density and
        then blitted every frame. Two dozen filled, overlapping slices are most
        of the drawing cost, and with a static patch they never change, so
        painting them only when they do is what keeps an idle display cheap.
        Only the live trace and its trail are drawn fresh each frame, and those
        sit on top of the terrain, so the layering is exact. */
    juce::Image terrainImage_;
    float terrainImageScale_ = 0.0f;
    bool terrainImageStale_ = true;

    std::array<TrailCycle, kTrailLength> trail_;
    int trailHead_ = 0;
    int trailCount_ = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WavetableDisplay)
};
