// Renders a short demo patch to a WAV file.
//
// The engine has no host dependency, so driving it offline needs nothing but a
// loop and a file writer. Useful for listening to a change without opening a
// DAW, and for producing the demo audio that goes alongside the write-ups.
//
//   render_demo [output.wav]

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "stankface/Params.h"
#include "stankface/WavetableEngine.h"

using namespace stankface;

namespace {

constexpr double kSampleRate = 48000.0;

void writeUint32(std::vector<uint8_t>& out, uint32_t value)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
}

void writeUint16(std::vector<uint8_t>& out, uint16_t value)
{
    for (int i = 0; i < 2; ++i)
        out.push_back(static_cast<uint8_t>((value >> (8 * i)) & 0xff));
}

void writeTag(std::vector<uint8_t>& out, const char* tag)
{
    for (int i = 0; i < 4; ++i)
        out.push_back(static_cast<uint8_t>(tag[i]));
}

bool writeWav(const std::string& path, const std::vector<float>& samples, double sampleRate)
{
    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * 2);

    std::vector<uint8_t> file;
    file.reserve(dataBytes + 64);

    writeTag(file, "RIFF");
    writeUint32(file, 36 + dataBytes);
    writeTag(file, "WAVE");

    writeTag(file, "fmt ");
    writeUint32(file, 16);
    writeUint16(file, 1);  // PCM
    writeUint16(file, 1);  // mono
    writeUint32(file, static_cast<uint32_t>(sampleRate));
    writeUint32(file, static_cast<uint32_t>(sampleRate) * 2);
    writeUint16(file, 2);
    writeUint16(file, 16);

    writeTag(file, "data");
    writeUint32(file, dataBytes);

    for (float sample : samples)
    {
        const float clamped = sample < -1.0f ? -1.0f : (sample > 1.0f ? 1.0f : sample);
        const int16_t value = static_cast<int16_t>(clamped * 32767.0f);
        writeUint16(file, static_cast<uint16_t>(value));
    }

    std::FILE* handle = std::fopen(path.c_str(), "wb");
    if (handle == nullptr)
        return false;

    const bool ok = std::fwrite(file.data(), 1, file.size(), handle) == file.size();
    std::fclose(handle);
    return ok;
}

void append(WavetableEngine& engine, std::vector<float>& out, double seconds)
{
    const int count = static_cast<int>(seconds * kSampleRate);
    const std::size_t start = out.size();
    out.resize(start + static_cast<std::size_t>(count));
    engine.renderBlock(out.data() + start, count);
}

/** One bar of a patch: a held note with the LFO doing the work. */
void renderSection(std::vector<float>& out, int table, float lfoRate,
                   float lfoToPosition, float lfoToCutoff,
                   const std::vector<int>& notes, double noteSeconds)
{
    WavetableEngine engine;
    engine.setSampleRate(kSampleRate);

    engine.setParam(ParamId::WavetableSelect, static_cast<float>(table));
    engine.setParam(ParamId::WavetablePosition, 0.4f);
    engine.setParam(ParamId::FilterCutoff, 700.0f);
    engine.setParam(ParamId::FilterResonance, 0.55f);
    engine.setParam(ParamId::Drive, 0.55f);
    engine.setParam(ParamId::AmpAttack, 0.004f);
    engine.setParam(ParamId::AmpDecay, 0.25f);
    engine.setParam(ParamId::AmpSustain, 0.85f);
    engine.setParam(ParamId::AmpRelease, 0.12f);
    engine.setParam(ParamId::LfoRate, lfoRate);
    engine.setParam(ParamId::LfoShape, static_cast<float>(LfoShape::Triangle));
    engine.setParam(ParamId::LfoToPosition, lfoToPosition);
    engine.setParam(ParamId::LfoToCutoff, lfoToCutoff);
    engine.setParam(ParamId::OutputGain, 0.7f);

    for (int note : notes)
    {
        engine.noteOn(note, 1.0f);
        append(engine, out, noteSeconds * 0.92);
        engine.noteOff(note);
        append(engine, out, noteSeconds * 0.08);
    }

    // Let the last release finish.
    append(engine, out, 0.3);
}

} // namespace

int main(int argc, char** argv)
{
    const std::string path = argc > 1 ? argv[1] : "stankface-demo.wav";

    std::vector<float> out;

    // Sub-to-saw morph under a slow wobble.
    renderSection(out, 0, 2.0f, 0.45f, 0.35f, { 29, 29, 36, 34 }, 0.6);

    // Reese: comb notches sweeping, faster wobble.
    renderSection(out, 1, 5.5f, 0.6f, 0.25f, { 31, 31, 38, 29 }, 0.6);

    // Formant growl, position modulated hard.
    renderSection(out, 2, 3.0f, 0.85f, 0.45f, { 28, 35, 28, 33 }, 0.6);

    if (!writeWav(path, out, kSampleRate))
    {
        std::fprintf(stderr, "could not write %s\n", path.c_str());
        return 1;
    }

    std::printf("wrote %s (%.1f s, %zu samples)\n", path.c_str(),
                static_cast<double>(out.size()) / kSampleRate, out.size());
    return 0;
}
