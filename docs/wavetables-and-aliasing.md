# Wavetables, band limiting, and the aliasing problem

A wavetable oscillator reads a stored single cycle at whatever rate the note
requires. That is cheap and it is why the technique exists — but read a table
containing a 500th harmonic at 3 kHz and that harmonic lands at 1.5 MHz, far
above what the sample rate can represent. It does not vanish. It folds back down
into the audible range at some unrelated frequency, and because the folded
partials are not harmonically related to the note, they do not sound like
brightness. They sound like a cheap synth: a metallic shimmer that moves the
wrong way when you play up the keyboard, and a wash of grit that appears exactly
when you modulate hard.

That last part is what makes it a real problem for this instrument rather than a
theoretical one. A pad holding one note is easy. A bass patch with an LFO
sweeping wavetable position several times a second, played across two octaves,
is the case where aliasing is loudest and most obvious.

## Band limiting by construction

The tables here are not recorded or drawn and then filtered. Each frame is
defined as a harmonic spectrum — an amplitude and phase per harmonic — and
synthesised additively by `wavetables/generate_wavetables.py`. Limiting the
bandwidth is then a matter of summing fewer harmonics, and the result is exactly
band-limited rather than approximately so. There is no filter to design and no
transition band to argue about.

Each frame is stored several times over, at descending harmonic limits. At
render time the oscillator picks the version whose content fits under Nyquist
for the note being played. This is the same idea as a texture mipmap, and the
name comes from there.

## Three decisions that turned out to matter

Getting the concept right is not enough; the details are where the noise floor
is actually decided. Three of them moved the measured result by tens of dB.

### The crossfade can only ever go darker

Levels have to be blended rather than switched, or a glide crossing a boundary
steps audibly as the band limit changes. The obvious way to blend is to compute
a fractional level and crossfade between the two either side of it.

That is wrong, and it is wrong in the worst possible way: it fades in aliasing.
Half an octave above a boundary, the brighter of the two neighbours is exactly
the level that was rejected for being unsafe, and the crossfade holds it at
50%. The partials above Nyquist are attenuated by 6 dB instead of removed.
Measured, this leaked at −20 to −40 dB depending on the table. Loud enough to
hear plainly.

The fix is to search for the brightest level that genuinely fits, and blend only
*downwards* from it, into the next level down, as the current one approaches its
limit. Both levels in the mix are then safe at all times, and the mix reaches
the darker one exactly at the boundary — so nothing steps.

The cost is that the level in use is always a little darker than strictly
necessary. Which leads to the second decision.

### Levels are spaced half an octave apart

With one-octave spacing, blending downwards gives up a whole octave of top end
at each boundary. Halving the spacing halves what is given up, at about 1.7x the
memory — a few hundred KB, which is not a real cost for an instrument.

Harmonic counts run 512, 362, 256, 181, 128, 91, … down to 1. They are rounded
to integers, which is why the oscillator finds its level by searching the table
rather than evaluating a closed form: a formula would be off by one around the
boundaries, and being off by one on the bright side is precisely the failure
this is all here to prevent.

Level 0 carries 512 harmonics, which reaches 20 kHz on a 40 Hz note. That
matters for this instrument specifically — the register it lives in is the one
where an over-eager anti-aliasing scheme quietly dulls everything.

### Linear interpolation is itself a distortion generator

This one was invisible until it was measured. After the mip selection was
correct, the spectrum still showed non-harmonic content at −55 dB — including on
a frame that is very nearly a pure sine, which cannot possibly alias from band
limiting, because there is nothing above the fundamental to fold.

The source was the interpolation between stored samples. Reading a table at a
fractional position and interpolating linearly introduces error, and that error
grows only as the *square* of the sample spacing. On the short tables used for
high notes it was producing more spurious content than the band limiting was
removing.

Switching to a four-point Catmull-Rom read, whose error falls as the fourth
power of spacing, moved the worst case from −64 dB to −82 dB — an 18 dB
improvement for a handful of multiplies per sample, and far more than the same
memory spent on longer tables would have bought. Each stored level is wrapped in
guard samples, one before and two after, so the four-point read needs no
wrap-around check in the inner loop.

## Measuring it

The test picks a fundamental that completes a whole number of cycles in the FFT
buffer, so every harmonic of the note lands exactly on a bin. No window is
applied and none is wanted — a window would smear each partial across its
neighbours and hide the thing being measured. Every bin that is *not* a multiple
of the fundamental is then, by construction, content that should not exist:
folded partials land at frequencies unrelated to the note.

The assertion is that all of it stays below −75 dB relative to the loudest
harmonic, checked across every wavetable, four morph positions, and two
registers — around 2.9 kHz, where only eight harmonics fit under Nyquist, and
around 41 Hz, where hundreds do.

Worst case measured is −82 dB. The threshold is deliberately close to it:
reverting the cubic read to linear costs about 18 dB, and getting the mip
selection off by one level costs 40 dB or more. Either regression fails the
suite immediately rather than quietly making the instrument sound worse.
