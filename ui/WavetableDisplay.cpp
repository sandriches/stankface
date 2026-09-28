#include "WavetableDisplay.h"

#include "StankfaceLookAndFeel.h"

using namespace stankface;

namespace {

// The same accent the knobs use, so the trace and the controls read as one
// instrument rather than two unrelated pieces of UI.
const juce::Colour kBackground = stankface_ui::colours::panel;
const juce::Colour kOutline    = stankface_ui::colours::outline;
const juce::Colour kTrace      = stankface_ui::colours::accent;

} // namespace

WavetableDisplay::WavetableDisplay(StankfaceAudioProcessor& processor)
    : processor_(processor)
{
    cycle_.assign(kCycleSamples, 0.0f);
    setOpaque(false);

    renderCycle();
    startTimerHz(kFramesPerSecond);
}

float WavetableDisplay::parameterValue(ParamId id) const
{
    const ParamDescriptor& descriptor = paramDescriptor(id);

    if (auto* value = processor_.parameters().getRawParameterValue(descriptor.id))
        return value->load();

    return descriptor.defaultValue;
}

void WavetableDisplay::renderCycle()
{
    // The editor can be open before the host has called prepareToPlay, so there
    // is not always a real rate to use yet.
    double sampleRate = processor_.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    if (! juce::approximatelyEqual(sampleRate, preparedSampleRate_))
    {
        preparedSampleRate_ = sampleRate;

        oscillator_.setSampleRate(sampleRate);
        filter_.setSampleRate(sampleRate);

        // One cycle is exactly kCycleSamples samples, so the phase returns to
        // zero at the end of every frame and the curve does not crawl.
        oscillator_.setFrequency(static_cast<float>(sampleRate / kCycleSamples));
        oscillator_.resetPhase();
        filter_.reset();
    }

    oscillator_.setTable(
        static_cast<int>(parameterValue(ParamId::WavetableSelect) + 0.5f));
    filter_.setResonance(parameterValue(ParamId::FilterResonance));
    filter_.setDrive(parameterValue(ParamId::Drive));

    // Position and cutoff come from the engine, after the LFO, rather than from
    // the parameters. This is what makes the drawing track a wobble.
    const WavetableEngine::DisplayState state = processor_.displaySnapshot();
    oscillator_.setPosition(state.wavetablePosition);
    filter_.setCutoff(state.filterCutoff);

    // The filter keeps its state between frames, so it is already settled. The
    // extra cycles only matter right after a knob moves, where they let the
    // curve catch up within one frame instead of over several.
    for (int cycle = 0; cycle < kCyclesPerFrame; ++cycle)
        for (int i = 0; i < kCycleSamples; ++i)
            cycle_[static_cast<std::size_t>(i)] =
                filter_.process(oscillator_.nextSample());
}

void WavetableDisplay::timerCallback()
{
    renderCycle();
    repaint();
}

void WavetableDisplay::paint(juce::Graphics& g)
{
    const juce::Rectangle<float> area = getLocalBounds().toFloat().reduced(1.0f);

    g.setColour(kBackground);
    g.fillRoundedRectangle(area, 4.0f);

    g.setColour(kOutline);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    const float centreY = area.getCentreY();

    // Without a zero line the curve cannot be read as a waveform.
    g.drawHorizontalLine(static_cast<int>(centreY), area.getX(), area.getRight());

    if (cycle_.size() < 2)
        return;

    const float lastIndex = static_cast<float>(cycle_.size() - 1);
    const float halfHeight = area.getHeight() * 0.5f - 2.0f;

    juce::Path trace;
    for (std::size_t i = 0; i < cycle_.size(); ++i)
    {
        const float x = area.getX()
                      + area.getWidth() * static_cast<float>(i) / lastIndex;

        const float clamped = juce::jlimit(-kVerticalRange, kVerticalRange, cycle_[i]);
        const float y = centreY - clamped / kVerticalRange * halfHeight;

        if (i == 0)
            trace.startNewSubPath(x, y);
        else
            trace.lineTo(x, y);
    }

    g.setColour(kTrace);
    g.strokePath(trace, juce::PathStrokeType(1.6f));
}
