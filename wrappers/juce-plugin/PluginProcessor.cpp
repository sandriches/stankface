#include "PluginProcessor.h"

#include "PluginEditor.h"
#include "stankface/WavetableData.h"

using namespace stankface;

namespace {

/** Parameters the host should show as a list rather than a continuous control. */
juce::StringArray choicesFor(ParamId id)
{
    if (id == ParamId::WavetableSelect)
    {
        juce::StringArray names;
        for (int i = 0; i < kNumWavetables; ++i)
            names.add(kWavetableNames[i]);
        return names;
    }

    if (id == ParamId::LfoShape)
        return { "Sine", "Triangle", "Saw", "Square" };

    if (id == ParamId::VoiceMode)
        return { "Mono", "Poly" };

    return {};
}

} // namespace

bool isChoiceParameter(ParamId id)
{
    return choicesFor(id).size() > 0;
}

juce::String formatParamValue(ParamId id, float naturalValue)
{
    const float scaled = naturalValue * paramDisplayScale(id);
    const int decimals = paramDisplayDecimals(id);

    // Rounded explicitly rather than left to juce::String(value, 0). Asking
    // that for zero decimal places does not round: it falls back to the
    // shortest representation that round-trips, so a 5 ms attack held as
    // 4.9999999f prints every one of those digits.
    const juce::String text = decimals > 0
        ? juce::String(scaled, decimals)
        : juce::String(juce::roundToInt(scaled));

    const juce::String unit(paramDescriptor(id).unit);

    if (unit.isEmpty())
        return text;

    // Percent sits tight against the number, the rest read better spaced.
    return unit == "%" ? text + unit : text + " " + unit;
}

float parseParamValue(ParamId id, const juce::String& text)
{
    const double value = text.retainCharacters("0123456789.,-+").getDoubleValue();
    return static_cast<float>(value) / paramDisplayScale(id);
}

juce::AudioProcessorValueTreeState::ParameterLayout
StankfaceAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    for (int i = 0; i < kNumParams; ++i)
    {
        const ParamId id = static_cast<ParamId>(i);
        const ParamDescriptor& d = paramDescriptor(id);

        // Version hint 1 throughout: this is the first release, and bumping it
        // later is what tells hosts a parameter was added rather than moved.
        const juce::ParameterID parameterId { d.id, 1 };

        const juce::StringArray choices = choicesFor(id);
        if (choices.size() > 0)
        {
            layout.add(std::make_unique<juce::AudioParameterChoice>(
                parameterId, d.name, choices,
                static_cast<int>(d.defaultValue + 0.5f)));
            continue;
        }

        // The range delegates to the engine's own curve rather than restating
        // it as a JUCE skew. The host's mapping is the one that actually drives
        // the plugin, so anything approximated here would silently override
        // what the descriptor says -- a logarithmic parameter would come out
        // linear and nothing would look wrong.
        juce::NormalisableRange<float> range {
            d.minValue, d.maxValue,
            [id](float, float, float normalised) {
                return paramFromNormalised(id, normalised);
            },
            [id](float, float, float natural) {
                return paramToNormalised(id, natural);
            }
        };

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            parameterId, d.name, range, d.defaultValue,
            juce::AudioParameterFloatAttributes()
                .withLabel(d.unit)
                .withStringFromValueFunction([id](float value, int) {
                    return formatParamValue(id, value);
                })
                .withValueFromStringFunction([id](const juce::String& text) {
                    return parseParamValue(id, text);
                })));
    }

    return layout;
}

StankfaceAudioProcessor::StankfaceAudioProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output",
                                                  juce::AudioChannelSet::stereo(),
                                                  true)),
      parameters_(*this, nullptr, "STANKFACE", createParameterLayout())
{
    for (int i = 0; i < kNumParams; ++i)
        paramValues_[i] = parameters_.getRawParameterValue(
            paramDescriptor(static_cast<ParamId>(i)).id);
}

void StankfaceAudioProcessor::prepareToPlay(double sampleRate, int)
{
    engine_.setSampleRate(sampleRate);
    pushParametersToEngine();
}

void StankfaceAudioProcessor::releaseResources()
{
    engine_.reset();
}

bool StankfaceAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannels() != 0)
        return false;

    const juce::AudioChannelSet& out = layouts.getMainOutputChannelSet();
    return out == juce::AudioChannelSet::mono()
        || out == juce::AudioChannelSet::stereo();
}

double StankfaceAudioProcessor::getTailLengthSeconds() const
{
    // The amplifier closes when the release finishes, so the release time is
    // the whole tail.
    return static_cast<double>(paramValues_[static_cast<int>(ParamId::AmpRelease)]->load());
}

void StankfaceAudioProcessor::pushParametersToEngine()
{
    for (int i = 0; i < kNumParams; ++i)
        engine_.setParam(static_cast<ParamId>(i), paramValues_[i]->load());
}

void StankfaceAudioProcessor::handleMidiMessage(const juce::MidiMessage& message)
{
    if (message.isNoteOn())
        engine_.noteOn(message.getNoteNumber(), message.getFloatVelocity());
    else if (message.isNoteOff())
        engine_.noteOff(message.getNoteNumber());
    else if (message.isAllNotesOff() || message.isAllSoundOff())
        engine_.reset();
}

void StankfaceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                           juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;

    // Parameters are read once per block. Anything that needs to move faster
    // than that -- the LFO -- is generated inside the engine per sample.
    pushParametersToEngine();

    const int numSamples = buffer.getNumSamples();
    float* const mono = buffer.getWritePointer(0);

    // Merge anything played on the on-screen keyboard into the MIDI buffer
    // before it is read, so those notes are indistinguishable from the host's
    // from here on. This also updates the keyboard's own state from the host's
    // notes, which is what makes its keys light up during playback.
    keyboardState_.processNextMidiBuffer(midi, 0, numSamples, true);

    // Split the block at each MIDI event so notes land on the sample they were
    // sent on rather than the start of the block.
    int position = 0;
    for (const juce::MidiMessageMetadata metadata : midi)
    {
        const int eventTime = juce::jlimit(0, numSamples, metadata.samplePosition);

        if (eventTime > position)
        {
            engine_.renderBlock(mono + position, eventTime - position);
            position = eventTime;
        }

        handleMidiMessage(metadata.getMessage());
    }

    if (position < numSamples)
        engine_.renderBlock(mono + position, numSamples - position);

    // The engine is mono; fan it out to whatever the host asked for.
    for (int channel = 1; channel < buffer.getNumChannels(); ++channel)
        buffer.copyFrom(channel, 0, mono, numSamples);

    // Hand the editor where the modulation ended up this block.
    const WavetableEngine::DisplayState state = engine_.displayState();
    displayPosition_.store(state.wavetablePosition, std::memory_order_relaxed);
    displayCutoff_.store(state.filterCutoff, std::memory_order_relaxed);
}

WavetableEngine::DisplayState StankfaceAudioProcessor::displaySnapshot() const
{
    WavetableEngine::DisplayState state;
    state.wavetablePosition = displayPosition_.load(std::memory_order_relaxed);
    state.filterCutoff = displayCutoff_.load(std::memory_order_relaxed);
    return state;
}

juce::AudioProcessorEditor* StankfaceAudioProcessor::createEditor()
{
    return new StankfaceAudioProcessorEditor(*this);
}

void StankfaceAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = parameters_.copyState().createXml())
        copyXmlToBinary(*xml, destData);
}

void StankfaceAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
        if (xml->hasTagName(parameters_.state.getType()))
            parameters_.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StankfaceAudioProcessor();
}
