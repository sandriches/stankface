#include "WavetableDisplay.h"

#include <cmath>
#include <iterator>

#include "StankfaceLookAndFeel.h"

using namespace stankface;

namespace {

const juce::Colour kBackground = stankface_ui::colours::panel;
const juce::Colour kOutline    = stankface_ui::colours::outline;

/** Cross-lines running back through the stack, as a count across one cycle.
    Divides the slice resolution, so every line lands on a stored point. */
constexpr int kMeshColumns = 32;

/** The colours the display is drawn in, worked out from the sound.

    Colour runs across the mesh, through hue along the cycle and back through
    the morph, the way the shader in a wireframe visualiser grades its plane.
    What sets it apart is that the whole palette moves with the patch: each
    table has its own base hue, and opening the filter or adding drive turns it
    warmer. Anything that sweeps the cutoff -- the LFO, the mod envelope, a
    hand on the knob -- makes the colours swim with the sound.
*/
struct Palette
{
    float baseHue = 0.0f;
    float heat = 0.0f;

    Palette(int table, float cutoffHz, float drive)
    {
        // Acid green for Growl so the default patch matches the knobs; the
        // other tables sit far enough round the wheel to tell apart at a
        // glance. Tables added later fall back to golden-ratio steps.
        static constexpr float tableHues[] = { 0.52f, 0.78f, 0.23f };
        constexpr int numTableHues = static_cast<int>(std::size(tableHues));

        const float tableHue = table >= 0 && table < numTableHues
            ? tableHues[table]
            : std::fmod(0.23f + 0.618f * static_cast<float>(table), 1.0f);

        // Cutoff on a log scale across the parameter's own 20 Hz - 20 kHz.
        const float cutoff = juce::jlimit(0.0f, 1.0f,
            std::log(juce::jmax(20.0f, cutoffHz) / 20.0f) / std::log(1000.0f));

        heat = juce::jlimit(0.0f, 1.0f, cutoff * 1.1f + drive * 0.35f - 0.35f);
        baseHue = tableHue - 0.22f * heat;
    }

    /** Colour of the mesh at a point, with distance fog. Time and depth are
        both 0..1; depth 1 is the back. */
    juce::Colour at(float time, float depth, float alpha = 1.0f) const
    {
        const float hue = baseHue + 0.34f * time - 0.22f * depth;
        const float saturation = 0.80f + 0.18f * heat;
        const float brightness = (1.0f - 0.50f * depth) * (0.85f + 0.15f * heat);

        return juce::Colour::fromHSV(hue - std::floor(hue), saturation,
                                     brightness, alpha);
    }

    /** The same colour without fog, and pushed towards white: for the live
        trace, which should burn brighter than anything in the terrain. */
    juce::Colour hot(float time, float whiteness, float alpha = 1.0f) const
    {
        return at(time, 0.0f).brighter(0.15f)
                             .interpolatedWith(juce::Colours::white, whiteness)
                             .withAlpha(alpha);
    }

    /** A horizontal gradient that carries the palette across one slice.
        Sampled at a handful of stops, which is plenty for a hue sweep this
        gentle. */
    template <typename ColourAt>
    static juce::ColourGradient across(float x0, float x1, ColourAt colourAt)
    {
        juce::ColourGradient gradient(colourAt(0.0f), x0, 0.0f,
                                      colourAt(1.0f), x1, 0.0f, false);

        for (int stop = 1; stop < 4; ++stop)
        {
            const float time = static_cast<float>(stop) / 4.0f;
            gradient.addColour(time, colourAt(time));
        }

        return gradient;
    }
};

/** How much narrower and shorter the back slice is than the front one. */
constexpr float kDepthShrink = 0.28f;

/** How far right the back slice starts, as a fraction of the front width. */
constexpr float kDepthSlant = 0.40f;

/** How far the back slice is lifted, as a fraction of the front amplitude. */
constexpr float kDepthRise = 0.90f;

/** Oblique projection of (depth, time, value) onto the panel.

    Depth 0 is the front slice and depth 1 the back. Not a true perspective
    divide: a linear shrink with depth reads the same at this size and keeps
    every slice a plain scaled copy of the front one.
*/
struct Projection
{
    float left = 0.0f;
    float frontZeroY = 0.0f;
    float frontWidth = 0.0f;
    float frontAmplitude = 0.0f;
    float valueRange = 1.0f;

    Projection(juce::Rectangle<float> area, float range)
        : valueRange(range)
    {
        // Sized so that the front slice's trough and the back slice's peak
        // both land exactly on the edges of the area.
        frontAmplitude = area.getHeight()
                       / (1.0f + kDepthRise + (1.0f - kDepthShrink));
        frontWidth = area.getWidth() / (kDepthSlant + 1.0f - kDepthShrink);
        left = area.getX();
        frontZeroY = area.getBottom() - frontAmplitude;
    }

    float scaleAt(float depth) const { return 1.0f - kDepthShrink * depth; }

    juce::Point<float> at(float depth, float time, float value) const
    {
        const float scale = scaleAt(depth);
        const float clamped = juce::jlimit(-valueRange, valueRange, value);

        return { left + depth * kDepthSlant * frontWidth + time * frontWidth * scale,
                 frontZeroY - depth * kDepthRise * frontAmplitude
                            - clamped / valueRange * frontAmplitude * scale };
    }
};

constexpr float kCorner = 4.0f;

/** The screen's outline, inset so its border stroke is not clipped. */
juce::Rectangle<float> screenBounds(juce::Rectangle<int> local)
{
    return local.toFloat().reduced(1.0f);
}

/** Where the plot sits inside the screen, clear of its edges. */
juce::Rectangle<float> plotBounds(juce::Rectangle<float> screen)
{
    return screen.reduced(10.0f, 8.0f);
}

template <typename Points>
juce::Path curveAt(const Projection& projection, float depth, const Points& points)
{
    juce::Path path;

    const float lastIndex = static_cast<float>(points.size() - 1);
    for (std::size_t i = 0; i < points.size(); ++i)
    {
        const juce::Point<float> p =
            projection.at(depth, static_cast<float>(i) / lastIndex, points[i]);

        if (i == 0)
            path.startNewSubPath(p);
        else
            path.lineTo(p);
    }

    return path;
}

/** Box-filters a whole cycle down to the fixed number of points a slice keeps.
    Averaging rather than picking every nth sample, so a narrow resonant spike
    shows up as a bump instead of flickering in and out between frames. */
template <std::size_t N>
void decimate(const std::vector<float>& cycle, std::array<float, N>& points)
{
    const std::size_t step = cycle.size() / N;

    for (std::size_t i = 0; i < N; ++i)
    {
        float sum = 0.0f;
        for (std::size_t j = 0; j < step; ++j)
            sum += cycle[i * step + j];

        points[i] = sum / static_cast<float>(step);
    }
}

} // namespace

WavetableDisplay::WavetableDisplay(StankfaceAudioProcessor& processor)
    : processor_(processor)
{
    static_assert(kCycleSamples % kSlicePoints == 0,
                  "slices are box-filtered down from whole cycles");
    static_assert(kCycleSamples % kLivePoints == 0,
                  "the live trace is box-filtered down from a whole cycle");

    cycle_.assign(kCycleSamples, 0.0f);
    terrainScratch_.assign(kCycleSamples, 0.0f);
    setOpaque(false);

    for (int i = 0; i < kNumSlices; ++i)
        slices_[static_cast<std::size_t>(i)].position =
            static_cast<float>(i) / static_cast<float>(kNumSlices - 1);

    timerCallback();
    startTimerHz(kFramesPerSecond);
}

float WavetableDisplay::parameterValue(ParamId id) const
{
    const ParamDescriptor& descriptor = paramDescriptor(id);

    if (auto* value = processor_.parameters().getRawParameterValue(descriptor.id))
        return value->load();

    return descriptor.defaultValue;
}

void WavetableDisplay::prepare(double sampleRate)
{
    preparedSampleRate_ = sampleRate;

    // One cycle is exactly kCycleSamples samples, so the phase returns to zero
    // at the end of every frame and the curves do not crawl.
    const float frequency = static_cast<float>(sampleRate / kCycleSamples);

    auto prepareChain = [&](WavetableOscillator& oscillator, Filter& filter) {
        oscillator.setSampleRate(sampleRate);
        filter.setSampleRate(sampleRate);
        oscillator.setFrequency(frequency);
        oscillator.resetPhase();
        filter.reset();
    };

    prepareChain(oscillator_, filter_);

    for (Slice& slice : slices_)
    {
        prepareChain(slice.oscillator, slice.filter);
        slice.oscillator.setPosition(slice.position);
    }

    // The filters were just cleared, so the terrain has to settle again even
    // if no parameter moved.
    settleFramesLeft_ = kSettleFrames;
}

void WavetableDisplay::renderCycle()
{
    // The filter keeps its state between frames, so it is already settled. The
    // extra cycles only matter right after a knob moves, where they let the
    // curve catch up within one frame instead of over several.
    for (int cycle = 0; cycle < kCyclesPerFrame; ++cycle)
        for (int i = 0; i < kCycleSamples; ++i)
            cycle_[static_cast<std::size_t>(i)] =
                filter_.process(oscillator_.nextSample());

    decimate(cycle_, livePoints_);

    TrailCycle& entry = trail_[static_cast<std::size_t>(trailHead_)];
    entry.position = livePosition_;
    decimate(cycle_, entry.points);

    trailHead_ = (trailHead_ + 1) % kTrailLength;
    trailCount_ = juce::jmin(trailCount_ + 1, kTrailLength);
}

void WavetableDisplay::renderTerrain(const TerrainInputs& inputs)
{
    // One cycle per frame per slice. Each filter carries its state between
    // frames like the live one does, so the terrain trails a knob by a frame
    // at most and needs no catching up.
    for (Slice& slice : slices_)
    {
        slice.oscillator.setTable(inputs.table);
        slice.filter.setResonance(inputs.resonance);
        slice.filter.setDrive(inputs.drive);
        slice.filter.setCutoff(inputs.cutoff);

        for (float& sample : terrainScratch_)
            sample = slice.filter.process(slice.oscillator.nextSample());

        decimate(terrainScratch_, slice.points);
    }

    terrainImageStale_ = true;
}

void WavetableDisplay::timerCallback()
{
    // The editor can be open before the host has called prepareToPlay, so there
    // is not always a real rate to use yet.
    double sampleRate = processor_.getSampleRate();
    if (sampleRate <= 0.0)
        sampleRate = 48000.0;

    if (! juce::approximatelyEqual(sampleRate, preparedSampleRate_))
        prepare(sampleRate);

    // Position and cutoff come from the engine, after the LFO, rather than from
    // the parameters. This is what makes the drawing track a wobble.
    const WavetableEngine::DisplayState state = processor_.displaySnapshot();

    TerrainInputs inputs;
    inputs.table = static_cast<int>(parameterValue(ParamId::WavetableSelect) + 0.5f);
    inputs.resonance = parameterValue(ParamId::FilterResonance);
    inputs.drive = parameterValue(ParamId::Drive);
    inputs.cutoff = state.filterCutoff;

    oscillator_.setTable(inputs.table);
    filter_.setResonance(inputs.resonance);
    filter_.setDrive(inputs.drive);
    filter_.setCutoff(inputs.cutoff);

    livePosition_ = juce::jlimit(0.0f, 1.0f, state.wavetablePosition);
    oscillator_.setPosition(livePosition_);

    renderCycle();

    // Only the cutoff carries the LFO into the terrain; position modulation
    // moves the live trace through it but leaves the slices alone.
    const bool lfoSweeping =
        ! juce::exactlyEqual(inputs.cutoff, terrainInputs_.cutoff)
        && ! juce::exactlyEqual(parameterValue(ParamId::LfoToCutoff), 0.0f);

    if (inputs != terrainInputs_)
    {
        terrainInputs_ = inputs;
        settleFramesLeft_ = kSettleFrames;
    }

    ++framesSinceTerrain_;

    if (settleFramesLeft_ > 0
        && (! lfoSweeping || framesSinceTerrain_ >= kSweepingTerrainInterval))
    {
        renderTerrain(inputs);
        --settleFramesLeft_;
        framesSinceTerrain_ = 0;
    }

    repaint();
}

void WavetableDisplay::paintTerrain(juce::Graphics& g) const
{
    const juce::Rectangle<float> bounds = screenBounds(getLocalBounds());
    const Palette colours(terrainInputs_.table, terrainInputs_.cutoff, terrainInputs_.drive);

    // A pool of light behind the terrain, tinted with the palette and falling
    // off to near black at the edges, so the display reads as a lit screen set
    // into the panel rather than a flat box.
    juce::ColourGradient vignette(kBackground.interpolatedWith(colours.at(0.5f, 0.5f), 0.14f),
                                  bounds.getCentreX(), bounds.getCentreY(),
                                  kBackground.darker(0.45f),
                                  bounds.getX(), bounds.getY(), true);
    g.setGradientFill(vignette);
    g.fillRoundedRectangle(bounds, kCorner);

    juce::Path screen;
    screen.addRoundedRectangle(bounds, kCorner);
    g.reduceClipRegion(screen);

    const Projection projection(plotBounds(bounds), kVerticalRange);

    // Graticule on the back wall, like the grid etched on a scope's face.
    // Behind everything, so the terrain occludes it where it rises.
    g.setColour(colours.at(0.5f, 1.0f, 0.28f));
    for (int i = -2; i <= 2; ++i)
    {
        const float value = static_cast<float>(i) * 0.5f;
        g.drawLine({ projection.at(1.0f, 0.0f, value),
                     projection.at(1.0f, 1.0f, value) }, 0.8f);
    }

    for (int i = 0; i <= 8; ++i)
    {
        const float time = static_cast<float>(i) / 8.0f;
        g.drawLine({ projection.at(1.0f, time, -kVerticalRange),
                     projection.at(1.0f, time, kVerticalRange) }, 0.8f);
    }

    constexpr int pointsPerColumn = kSlicePoints / kMeshColumns;
    static_assert(kSlicePoints % kMeshColumns == 0,
                  "mesh columns have to land on stored points");

    const auto pointAt = [&](int sliceIndex, int column) {
        const Slice& slice = slices_[static_cast<std::size_t>(sliceIndex)];
        const int index = juce::jmin(column * pointsPerColumn, kSlicePoints - 1);

        return projection.at(slice.position,
                             static_cast<float>(column) / kMeshColumns,
                             slice.points[static_cast<std::size_t>(index)]);
    };

    // Painted back to front, as a mesh. For each slice, the cross-lines and
    // diagonals linking it to the slice behind go down first, then the slice
    // is filled down to the bottom of its slab and its line drawn on top. The
    // fill hides whatever is behind and below it, cross-lines included:
    // hidden-line removal by painter's algorithm, which is what keeps a mesh
    // this dense legible instead of a see-through tangle.
    for (int i = kNumSlices - 1; i >= 0; --i)
    {
        const Slice& slice = slices_[static_cast<std::size_t>(i)];
        const float depth = slice.position;

        const float x0 = projection.at(depth, 0.0f, 0.0f).x;
        const float x1 = projection.at(depth, 1.0f, 0.0f).x;

        if (i < kNumSlices - 1)
        {
            juce::Path rungs;
            juce::Path diagonals;

            for (int column = 0; column <= kMeshColumns; ++column)
            {
                rungs.startNewSubPath(pointAt(i + 1, column));
                rungs.lineTo(pointAt(i, column));

                if (column < kMeshColumns)
                {
                    diagonals.startNewSubPath(pointAt(i + 1, column));
                    diagonals.lineTo(pointAt(i, column + 1));
                }
            }

            const float between = (depth + slices_[static_cast<std::size_t>(i + 1)].position) * 0.5f;

            g.setGradientFill(Palette::across(x0, x1, [&](float time) {
                return colours.at(time, between, 0.20f);
            }));
            g.strokePath(diagonals, juce::PathStrokeType(0.6f));

            g.setGradientFill(Palette::across(x0, x1, [&](float time) {
                return colours.at(time, between, 0.55f);
            }));
            g.strokePath(rungs, juce::PathStrokeType(0.8f));
        }

        const juce::Path curve = curveAt(projection, depth, slice.points);

        juce::Path body(curve);
        body.lineTo(projection.at(depth, 1.0f, -kVerticalRange));
        body.lineTo(projection.at(depth, 0.0f, -kVerticalRange));
        body.closeSubPath();

        // Tinted where the wave runs high and dark down the slab, so the
        // surface glows faintly through its own fill.
        const juce::Point<float> top = projection.at(depth, 0.0f, kVerticalRange);
        const juce::Point<float> bottom = projection.at(depth, 0.0f, -kVerticalRange);

        g.setGradientFill(juce::ColourGradient(
            kBackground.interpolatedWith(colours.at(0.5f, depth), 0.22f).withAlpha(0.93f), top,
            kBackground.darker(0.5f).withAlpha(0.93f), bottom, false));
        g.fillPath(body);

        g.setGradientFill(Palette::across(x0, x1, [&](float time) {
            return colours.at(time, depth, 0.95f);
        }));
        g.strokePath(curve, juce::PathStrokeType(1.1f));
    }
}

void WavetableDisplay::paint(juce::Graphics& g)
{
    // The editor scales its contents with a transform and the screen may be
    // high density, so the cache is drawn at the real pixel scale to stay as
    // sharp as drawing directly would be.
    const float scale = g.getInternalContext().getPhysicalPixelScaleFactor();
    const int imageWidth = juce::roundToInt(static_cast<float>(getWidth()) * scale);
    const int imageHeight = juce::roundToInt(static_cast<float>(getHeight()) * scale);

    if (imageWidth <= 0 || imageHeight <= 0)
        return;

    if (terrainImageStale_
        || ! juce::approximatelyEqual(scale, terrainImageScale_)
        || terrainImage_.getWidth() != imageWidth
        || terrainImage_.getHeight() != imageHeight)
    {
        // Drawn with JUCE's own renderer, then handed to the platform once.
        // The mesh is about a hundred gradient strokes, and CoreGraphics does
        // each of those as a clip plus a gradient over the whole bounding box,
        // which made a sweeping terrain cost twice as much. Drawing it in
        // software fills only the pixels each stroke covers. The conversion
        // matters the other way round: blitting a software image converts it
        // on every frame, so the native copy is what keeps a still terrain
        // as cheap to show as it was before.
        juce::Image rendered(juce::Image::ARGB, imageWidth, imageHeight, true,
                             juce::SoftwareImageType());
        {
            juce::Graphics terrain(rendered);
            terrain.addTransform(juce::AffineTransform::scale(scale));
            paintTerrain(terrain);
        }

        terrainImage_ = juce::NativeImageType().convert(rendered);
        terrainImageScale_ = scale;
        terrainImageStale_ = false;
    }

    g.drawImageTransformed(terrainImage_, juce::AffineTransform::scale(1.0f / scale));

    const juce::Rectangle<float> bounds = screenBounds(getLocalBounds());
    const Palette colours(terrainInputs_.table, terrainInputs_.cutoff, terrainInputs_.drive);

    {
        juce::Graphics::ScopedSaveState clipped(g);

        juce::Path screen;
        screen.addRoundedRectangle(bounds, kCorner);
        g.reduceClipRegion(screen);

        const Projection projection(plotBounds(bounds), kVerticalRange);
        const float liveDepth = livePosition_;

        const float x0 = projection.at(liveDepth, 0.0f, 0.0f).x;
        const float x1 = projection.at(liveDepth, 1.0f, 0.0f).x;

        const auto liveGradient = [&](float whiteness, float alpha) {
            return Palette::across(x0, x1, [&](float time) {
                return colours.hot(time, whiteness, alpha);
            });
        };

        // The live slice's zero line, plus a marker on the left rail, so where
        // the morph sits in the stack reads even when the trace is near flat.
        g.setGradientFill(liveGradient(0.0f, 0.4f));
        g.drawLine({ projection.at(liveDepth, 0.0f, 0.0f),
                     projection.at(liveDepth, 1.0f, 0.0f) }, 1.0f);

        {
            const juce::Point<float> tip = projection.at(liveDepth, 0.0f, 0.0f);

            juce::Path marker;
            marker.addTriangle(tip.translated(-1.0f, 0.0f),
                               tip.translated(-7.0f, -4.0f),
                               tip.translated(-7.0f, 4.0f));
            g.setColour(colours.hot(0.0f, 0.2f));
            g.fillPath(marker);
        }

        // Phosphor trail: the last few live cycles, oldest faintest. A held
        // patch stacks them on the trace itself; a wobble smears them through
        // the terrain the way persistence smears a moving trace on a scope.
        for (int age = trailCount_ - 1; age >= 1; --age)
        {
            const int index = (trailHead_ - 1 - age + kTrailLength) % kTrailLength;
            const TrailCycle& entry = trail_[static_cast<std::size_t>(index)];

            const float fade = 1.0f - static_cast<float>(age)
                                    / static_cast<float>(kTrailLength);

            g.setGradientFill(liveGradient(0.0f, 0.25f * fade * fade));
            g.strokePath(curveAt(projection, entry.position, entry.points),
                         juce::PathStrokeType(1.2f));
        }

        // The live cycle: a fill out to the zero line that fades towards it, so
        // the trace has body, then a faint halo under a hot core.
        const juce::Path live = curveAt(projection, liveDepth, livePoints_);

        {
            juce::Path area(live);
            area.lineTo(projection.at(liveDepth, 1.0f, 0.0f));
            area.lineTo(projection.at(liveDepth, 0.0f, 0.0f));
            area.closeSubPath();

            const juce::Point<float> top = projection.at(liveDepth, 0.0f, kVerticalRange);
            const juce::Point<float> bottom = projection.at(liveDepth, 0.0f, -kVerticalRange);
            const juce::Colour tint = colours.hot(0.5f, 0.0f);

            juce::ColourGradient body(tint.withAlpha(0.32f), top,
                                      tint.withAlpha(0.32f), bottom, false);
            body.addColour(0.5, tint.withAlpha(0.02f));

            g.setGradientFill(body);
            g.fillPath(area);
        }

        const juce::PathStrokeType::JointStyle joint = juce::PathStrokeType::curved;

        g.setGradientFill(liveGradient(0.0f, 0.14f));
        g.strokePath(live, juce::PathStrokeType(6.0f, joint));
        g.setGradientFill(liveGradient(0.1f, 0.5f));
        g.strokePath(live, juce::PathStrokeType(2.6f, joint));
        g.setGradientFill(liveGradient(0.55f, 1.0f));
        g.strokePath(live, juce::PathStrokeType(1.2f, joint));
    }

    g.setColour(kOutline);
    g.drawRoundedRectangle(bounds, kCorner, 1.0f);
}
