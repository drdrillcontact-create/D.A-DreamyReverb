// D.A AURA - DSP core, plain C++ (no JUCE dependency)
//
// Signal flow:
//   in -> pre-delay -> (+ shimmer return) -> 4x allpass diffusion
//      -> 8-line modulated FDN (Hadamard matrix, damping in the loop)
//      -> width -> equal-power dry/wet mix
//
//   Shimmer path (per channel):
//   FDN output -> high-pass -> anti-alias low-pass -> two granular pitch voices
//              -> tone filter -> safety limiter -> cross-fed back into the input
//
// The shimmer send level follows the decay time, so the loop gain of the
// shimmer path only depends on the Shimmer knob and stays below 1 for every
// Decay / Size setting.
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace dreamy
{

//==============================================================================
class DelayLine
{
public:
    void init (int minLength)
    {
        int size = 16;
        while (size < minLength + 8) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }

    void clear()                        { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x)         { buf[(size_t) w] = x; w = (w + 1) & mask; }

    // delay in samples, measured back from the most recently written sample (d >= 1)
    inline float read (int d) const     { return buf[(size_t) ((w - d) & mask)]; }

    // linear interpolation, d >= 1
    inline float readLinear (float d) const
    {
        const int   di = (int) d;
        const float fr = d - (float) di;
        const float a = buf[(size_t) ((w - di) & mask)];
        const float b = buf[(size_t) ((w - di - 1) & mask)];
        return a + fr * (b - a);
    }

    // 3rd-order Lagrange interpolation, d >= 2.
    // Its magnitude response never exceeds 1, so it is safe inside feedback loops.
    inline float readCubic (float d) const
    {
        const int   di = (int) d;
        const float t  = d - (float) di;
        const float ym = buf[(size_t) ((w - di + 1) & mask)];
        const float y0 = buf[(size_t) ((w - di)     & mask)];
        const float y1 = buf[(size_t) ((w - di - 1) & mask)];
        const float y2 = buf[(size_t) ((w - di - 2) & mask)];

        const float tm1 = t - 1.0f, tm2 = t - 2.0f, tp1 = t + 1.0f;
        const float cm = -t * tm1 * tm2 * (1.0f / 6.0f);
        const float c0 =  tp1 * tm1 * tm2 * 0.5f;
        const float c1 = -tp1 * t * tm2 * 0.5f;
        const float c2 =  tp1 * t * tm1 * (1.0f / 6.0f);
        return cm * ym + c0 * y0 + c1 * y1 + c2 * y2;
    }

private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

//==============================================================================
class Allpass
{
public:
    void init (int lengthSamples)       { len = std::max (1, lengthSamples); line.init (len); }
    void clear()                        { line.clear(); }
    inline float process (float x, float g)
    {
        const float d = line.read (len);
        const float y = d - g * x;
        line.write (x + g * y);
        return y;
    }
private:
    DelayLine line;
    int len = 1;
};

//==============================================================================
// 4th-order Butterworth low-pass (two biquads, transposed direct form II)
class Lowpass4
{
public:
    void setCutoff (double cutoffHz, double sampleRate)
    {
        static const double q[2] = { 0.541196100146197, 1.306562964876377 };
        const double w0 = 6.283185307179586 * std::min (cutoffHz, 0.45 * sampleRate) / sampleRate;
        const double cs = std::cos (w0), sn = std::sin (w0);

        for (int i = 0; i < 2; ++i)
        {
            const double alpha = sn / (2.0 * q[i]);
            const double a0 = 1.0 + alpha;
            b0[i] = (float) ((1.0 - cs) * 0.5 / a0);
            b1[i] = (float) ((1.0 - cs) / a0);
            a1[i] = (float) (-2.0 * cs / a0);
            a2[i] = (float) ((1.0 - alpha) / a0);
        }
    }

    void clear()                        { z1[0] = z1[1] = z2[0] = z2[1] = 0.0f; }

    inline float process (float x)
    {
        for (int i = 0; i < 2; ++i)
        {
            const float y = b0[i] * x + z1[i];
            z1[i] = b1[i] * x - a1[i] * y + z2[i];
            z2[i] = b0[i] * x - a2[i] * y;       // b2 == b0
            x = y;
        }
        return x;
    }

private:
    float b0[2] { 1.0f, 1.0f }, b1[2] {}, a1[2] {}, a2[2] {};
    float z1[2] {}, z2[2] {};
};

//==============================================================================
// One pitch voice: band-limits its input, then reads it back with four
// overlapping Hann-windowed grains (75 % overlap).
//
//  * Four Hann grains a quarter apart add up to a constant, so there is no
//    tremolo at the grain rate.
//  * Every new grain searches for the start position that lines up best with
//    the grain that is already playing (normalised cross-correlation, like a
//    studio "de-glitched" harmoniser). Notes and chords come out as clean,
//    steady octaves instead of a grainy flutter; noise-like material still gets
//    a random, choir-like spread.
//  * The output level is corrected with the measured correlation, so the voice
//    has unity power gain for tones and for noise alike.
//  * The input is low-passed below Nyquist / ratio first, so shifting up never
//    aliases.
//  * A new pitch ratio is picked up grain by grain, so mode changes never click.
class PitchVoice
{
public:
    static constexpr int numGrains = 4;
    static constexpr int tableSize = 1024;

    void init (double sampleRate, double grainMs, float maxRatio, std::uint32_t seed)
    {
        sr          = sampleRate;
        grainLen    = (float) (grainMs * 0.001 * sampleRate);
        inc         = 1.0f / grainLen;

        // search grid for the splice alignment (constant cost at every sample rate)
        step        = std::max (1, (int) (sampleRate / 48000.0 * 4.0 + 0.5));
        numCands    = (int) (0.012 * sampleRate) / step;      // 12 ms of start positions
        numTaps     = (int) (0.016 * sampleRate) / step;      // compared over 16 ms
        jitterRange = (float) (numCands * step);

        const float maxSweep = std::max (maxRatio - 1.0f, 0.5f) * grainLen;
        line.init ((int) (minDelay + jitterRange + maxSweep) + (numTaps + 2) * step + 16);

        rng = seed != 0 ? seed : 0x9E3779B9u;
        targetRatio = 2.0f;
        filterRatio = -1.0f;
        setRatio (2.0f);
        coherence = 0.0f;
        gain = 0.81649658f;

        for (int g = 0; g < numGrains; ++g)
        {
            phase[g]  = (float) g / (float) numGrains;
            ratio[g]  = targetRatio;
            jitter[g] = (float) (int) (nextRandom() * jitterRange);
        }
    }

    void clear()                        { line.clear(); filter.clear(); coherence = 0.0f; }

    void setRatio (float newRatio)
    {
        targetRatio = newRatio;

        if (newRatio != filterRatio)
        {
            filterRatio = newRatio;
            // keep everything the voice reads below Nyquist after shifting
            const double cutoff = sr * std::min (0.21, 0.38 / (double) std::max (newRatio, 1.0f));
            filter.setCutoff (cutoff, sr);
        }
    }

    // hann: table with (tableSize + 1) points of 0.5 - 0.5 cos (2 pi x)
    inline float process (float x, const float* hann)
    {
        line.write (filter.process (x));

        float out = 0.0f;
        for (int g = 0; g < numGrains; ++g)
        {
            float p = phase[g] + inc;
            if (p >= 1.0f)
            {
                p -= 1.0f;
                startGrain (g, p);
            }
            phase[g] = p;

            const float tp = p * (float) tableSize;
            const int   ti = (int) tp;
            const float win = hann[ti] + (tp - (float) ti) * (hann[ti + 1] - hann[ti]);

            out += win * line.readCubic (grainDelay (g, p));
        }

        return out * gain;
    }

private:
    inline float sweep (float r, float p) const
    {
        return r >= 1.0f ? (r - 1.0f) * grainLen * (1.0f - p)
                         : (1.0f - r) * grainLen * p;
    }

    inline float grainDelay (int g, float p) const   { return minDelay + jitter[g] + sweep (ratio[g], p); }

    // A new grain begins: latch the pitch and pick the start position that matches
    // the grain before it best.
    void startGrain (int g, float p)
    {
        // the grain that started a quarter of a grain ago
        const int ref = (g + 1) % numGrains;
        ratio[g] = targetRatio;

        const int refDelay  = (int) (grainDelay (ref, phase[ref]) + 0.5f);
        const int baseDelay = (int) (minDelay + sweep (ratio[g], p) + 0.5f);

        float refEnergy = 1.0e-12f;
        for (int i = 0; i < numTaps; ++i)
        {
            const float a = line.read (refDelay + i * step);
            refEnergy += a * a;
        }

        int   best = (int) (nextRandom() * jitterRange);   // used when there is nothing to match
        float bestScore = 0.0f;

        if (refEnergy > 1.0e-9f && ratio[ref] == ratio[g])
        {
            // coarse search over the whole range, then a fine search around the winner
            for (int pass = 0; pass < 2; ++pass)
            {
                const int from = pass == 0 ? 0 : std::max (0, best - step + 1);
                const int to   = pass == 0 ? numCands * step : std::min (numCands * step, best + step);
                const int inc2 = pass == 0 ? step : 1;

                for (int c = from; c < to; c += inc2)
                {
                    const int d = baseDelay + c;
                    float corr = 0.0f, energy = 1.0e-12f;
                    for (int i = 0; i < numTaps; ++i)
                    {
                        const float a = line.read (refDelay + i * step);
                        const float b = line.read (d + i * step);
                        corr   += a * b;
                        energy += b * b;
                    }
                    const float score = corr / std::sqrt (energy * refEnergy);
                    if (score > bestScore) { bestScore = score; best = c; }
                }
            }
        }

        jitter[g] = (float) best;

        // Level correction. With a correlation c between neighbouring grains the four
        // Hann grains carry (1.5 + 2 c + 0.5 c^2) times the input power.
        const float c = bestScore > 1.0f ? 1.0f : bestScore;
        coherence += 0.12f * (c - coherence);
        gain = 1.0f / std::sqrt (1.5f + 2.0f * coherence + 0.5f * coherence * coherence);
    }

    inline float nextRandom()           // 0..1
    {
        rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5;
        return (float) (rng >> 8) * (1.0f / 16777216.0f);
    }

    static constexpr float minDelay = 3.0f;

    DelayLine line;
    Lowpass4 filter;
    double sr = 48000.0;
    float grainLen = 6000.0f, inc = 0.0f, jitterRange = 0.0f;
    int   step = 4, numCands = 144, numTaps = 192;
    float targetRatio = 2.0f, filterRatio = -1.0f;
    float coherence = 0.0f, gain = 0.81649658f;
    float phase[numGrains] {}, ratio[numGrains] {}, jitter[numGrains] {};
    std::uint32_t rng = 1;
};

//==============================================================================
class Reverb
{
public:
    static constexpr int numLines = 8;

    enum ShimmerMode { octave = 0, octaveFifth, twoOctaves, subOctave, numShimmerModes };

    struct Params
    {
        float mix         = 0.4f;    // 0..1
        float size        = 0.6f;    // 0..1
        float decay       = 6.0f;    // seconds (RT60)
        float preDelay    = 20.0f;   // ms (0..500)
        float highCut     = 7000.0f; // Hz, damping inside the loop
        float lowCut      = 120.0f;  // Hz, inside the loop
        float modDepth    = 0.4f;    // 0..1
        float modRate     = 0.5f;    // Hz
        float shimmer     = 0.25f;   // 0..1
        float width       = 1.0f;    // 0..1.5
        int   shimmerMode = octave;  // ShimmerMode
        bool  freeze      = false;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        const double k = sr / 48000.0;

        static const int baseLen[numLines] = { 1487, 1699, 1933, 2213, 2539, 2903, 3271, 3697 };
        for (int i = 0; i < numLines; ++i)
        {
            lenBase[i] = (float) (baseLen[i] * k);
            lines[i].init ((int) (lenBase[i] * maxSizeScale + 64.0 * k) + 16);
            lfoPhase[i] = (float) i / (float) numLines;
        }

        static const int apL[4] = { 211, 317, 467, 743 };
        static const int apR[4] = { 229, 337, 491, 709 };
        for (int i = 0; i < 4; ++i)
        {
            diffL[i].init ((int) (apL[i] * k));
            diffR[i].init ((int) (apR[i] * k));
        }

        preL.init ((int) (0.52 * sr));
        preR.init ((int) (0.52 * sr));

        // different grain lengths per voice and per side keep the four shifters decorrelated
        voiceA[0].init (sr, 131.0, 2.01f, 0x1234ABCDu);
        voiceA[1].init (sr, 149.0, 2.01f, 0x7F4A7C15u);
        voiceB[0].init (sr, 173.0, 4.01f, 0xC0FFEE11u);
        voiceB[1].init (sr, 197.0, 4.01f, 0x51ED270Bu);
        currentMode = -1;

        for (int i = 0; i <= PitchVoice::tableSize; ++i)
            hann[i] = (float) (0.5 - 0.5 * std::cos (6.283185307179586 * (double) i / (double) PitchVoice::tableSize));

        smoothFast   = 1.0f - std::exp (-1.0f / (float) (0.02 * sr));
        smoothSlow   = 1.0f - std::exp (-1.0f / (float) (0.25 * sr));
        shimToneCoef = 1.0f - std::exp (-6.2831853f * 8000.0f / (float) sr);
        shimHpCoef   = 1.0f - std::exp (-6.2831853f * 160.0f / (float) sr);

        fadeStep = 1.0f / (float) (0.01 * sr);
        wetFade = 1.0f;
        flushing = false;
        flushRequested.store (false);

        reset();
        snapSmoothers = true;
        prepared = true;
    }

    bool isPrepared() const noexcept { return prepared; }

    void reset()
    {
        for (int i = 0; i < numLines; ++i) { lines[i].clear(); lp[i] = hp1[i] = hp2[i] = 0.0f; }
        for (int i = 0; i < 4; ++i)        { diffL[i].clear(); diffR[i].clear(); }
        preL.clear(); preR.clear();
        clearShimmer();
    }

    void setParams (const Params& p) { params = p; }

    // Fades the tail out (10 ms), empties the reverb, jumps to the current settings and
    // fades back in. Used when a preset is loaded, so the old tail does not glide in pitch
    // while the room size changes. Safe to call from any thread.
    void flush() noexcept            { flushRequested.store (true); }

    // In-place safe (inL may equal outL etc.)
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        // a host that processes before prepare() gets the dry signal instead of a crash
        if (! prepared)
        {
            for (int n = 0; n < numSamples; ++n) { const float l = inL[n], r = inR[n]; outL[n] = l; outR[n] = r; }
            return;
        }

        const float fsr = (float) sr;

        // ---- per-block targets -------------------------------------------------
        const bool  frozen   = params.freeze;
        const float tFreeze  = frozen ? 1.0f : 0.0f;
        const float tMix     = clamp (params.mix, 0.0f, 1.0f);
        const float tDry     = std::cos (tMix * 1.5707963f);
        const float tWet     = std::sin (tMix * 1.5707963f);
        const float tSize    = minSizeScale + (maxSizeScale - minSizeScale) * clamp (params.size, 0.0f, 1.0f);
        const float tPre     = 1.0f + clamp (params.preDelay, 0.0f, 500.0f) * 0.001f * fsr;
        const float tDepth   = clamp (params.modDepth, 0.0f, 1.0f) * 16.0f * fsr / 48000.0f;
        const float tWidth   = clamp (params.width, 0.0f, 1.5f);

        const float decay    = clamp (params.decay, 0.1f, 60.0f);
        const float tLpCoef  = 1.0f - std::exp (-6.2831853f * clamp (params.highCut, 200.0f, 0.45f * fsr) / fsr);
        const float tHpG     = std::tan (3.14159265f * clamp (params.lowCut, 5.0f, 2000.0f) / fsr);

        float tGain[numLines], lfoInc[numLines];
        float loss = 0.0f;   // average share of the energy a line loses per round trip
        const float rate = clamp (params.modRate, 0.01f, 10.0f);
        for (int i = 0; i < numLines; ++i)
        {
            const float lenSec = lenBase[i] * tSize / fsr;
            tGain[i]  = std::pow (10.0f, -3.0f * lenSec / decay);
            lfoInc[i] = rate * (1.0f + 0.083f * (float) i) / fsr;
            loss += 1.0f - tGain[i] * tGain[i];
        }
        loss *= 1.0f / (float) numLines;

        // Shimmer: "amount" is the loop gain of the pitch path (below 1 by design).
        // The tail holds 1 / (4 * loss) times the power that is fed in, so the send
        // is scaled by 2 * sqrt (loss) to compensate.
        const float amount   = clamp (params.shimmer, 0.0f, 1.0f);
        const float loopGain = amount > 0.0f ? maxShimmerLoopGain * std::pow (amount, 0.7f) : 0.0f;
        const float tShim    = frozen ? 0.0f : loopGain * 2.0f * std::sqrt (loss);

        const int mode = params.shimmerMode < 0 ? 0
                       : (params.shimmerMode >= numShimmerModes ? numShimmerModes - 1 : params.shimmerMode);
        if (mode != currentMode)
        {
            currentMode = mode;
            // { ratio A, ratio B, level A, level B } (levels are power-normalised)
            static const float table[numShimmerModes][4] =
            {
                { 2.0f, 2.0f,       1.0f, 0.0f },  // octave (single voice per side)
                { 2.0f, 1.4983071f, 0.8f, 0.6f },  // octave + fifth
                { 2.0f, 4.0f,       0.8f, 0.6f },  // octave + two octaves
                { 2.0f, 0.5f,       0.8f, 0.6f },  // octave + sub octave
            };
            // left and right are tuned 1.5 cents apart in opposite directions:
            // no beating inside a channel, but a wide, slowly moving stereo image
            static const float detune[2] = { 1.0008668f, 0.9991340f };
            for (int c = 0; c < 2; ++c)
            {
                voiceA[c].setRatio (table[mode][0] * detune[c]);
                voiceB[c].setRatio (table[mode][1] * detune[1 - c]);
            }
            tLevelA = table[mode][2];
            tLevelB = table[mode][3];
        }

        if (snapSmoothers)
        {
            sDry = tDry; sWet = tWet; sSize = tSize; sPre = tPre; sDepth = tDepth;
            sShim = tShim; sWidth = tWidth; sFreeze = tFreeze;
            sLpCoef = tLpCoef; sHpG = tHpG; sLevelA = tLevelA; sLevelB = tLevelB;
            for (int i = 0; i < numLines; ++i) sGain[i] = tGain[i];
            snapSmoothers = false;
        }

        // the pitch voices only run while they are audible
        const bool shimmerOn = tShim > 0.0f || sShim > 1.0e-5f;
        if (! shimmerOn && shimmerWasOn)
            clearShimmer();
        shimmerWasOn = shimmerOn;

        const bool voiceBOn = shimmerOn && (tLevelB > 0.0f || sLevelB > 1.0e-4f);
        if (! voiceBOn && voiceBWasOn)
        {
            voiceB[0].clear();
            voiceB[1].clear();
        }
        voiceBWasOn = voiceBOn;

        if (flushRequested.exchange (false))
            flushing = true;

        const float diffG = 0.62f;
        const float hadamardScale = 0.35355339f; // 1/sqrt(8)

        for (int n = 0; n < numSamples; ++n)
        {
            sDry    += smoothFast * (tDry    - sDry);
            sWet    += smoothFast * (tWet    - sWet);
            sWidth  += smoothFast * (tWidth  - sWidth);
            sShim   += smoothFast * (tShim   - sShim);
            sFreeze += smoothFast * (tFreeze - sFreeze);
            sDepth  += smoothFast * (tDepth  - sDepth);
            sLpCoef += smoothFast * (tLpCoef - sLpCoef);
            sHpG    += smoothFast * (tHpG    - sHpG);
            sLevelA += smoothFast * (tLevelA - sLevelA);
            sLevelB += smoothFast * (tLevelB - sLevelB);
            sSize   += smoothSlow * (tSize   - sSize);
            sPre    += smoothSlow * (tPre    - sPre);

            if (flushing)
            {
                wetFade -= fadeStep;
                if (wetFade <= 0.0f)
                {
                    wetFade = 0.0f;
                    flushing = false;
                    reset();
                    sSize = tSize; sPre = tPre; sDepth = tDepth; sShim = tShim; sFreeze = tFreeze;
                    sLpCoef = tLpCoef; sHpG = tHpG; sLevelA = tLevelA; sLevelB = tLevelB;
                    for (int i = 0; i < numLines; ++i) sGain[i] = tGain[i];
                }
            }
            else if (wetFade < 1.0f)
            {
                wetFade = std::min (1.0f, wetFade + fadeStep);
            }

            // hosts sometimes hand over uninitialised buffers: never let NaN / inf into the loops
            float dryL = inL[n];
            float dryR = inR[n];
            if (! (std::fabs (dryL) < 1.0e4f)) dryL = 0.0f;
            if (! (std::fabs (dryR) < 1.0e4f)) dryR = 0.0f;
            const float inGain = 1.0f - sFreeze;

            // pre-delay
            preL.write (dryL);
            preR.write (dryR);
            float xL = preL.readLinear (sPre) * inGain + shimRetL * sShim;
            float xR = preR.readLinear (sPre) * inGain + shimRetR * sShim;

            // input diffusion
            for (int i = 0; i < 4; ++i)
            {
                xL = diffL[i].process (xL, diffG);
                xR = diffR[i].process (xR, diffG);
            }

            // low cut: 2nd-order Butterworth high-pass (state variable form). Its flat
            // pass band leaves the decay time above the cutoff untouched.
            const float hpA1 = 1.0f / (1.0f + sHpG * (sHpG + 1.41421356f));
            const float hpA2 = sHpG * hpA1;
            const float hpA3 = sHpG * hpA2;

            // read the modulated delay lines, damp and attenuate
            float d[numLines], m[numLines];
            for (int i = 0; i < numLines; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float mod = sDepth * (1.0f + fastSine (lfoPhase[i]));
                const float raw = lines[i].readCubic (3.0f + lenBase[i] * sSize + mod);

                // the damping filters keep running while frozen, so releasing Freeze is click free
                lp[i] += sLpCoef * (raw - lp[i]);
                const float v3 = lp[i] - hp2[i];
                const float v1 = hpA1 * hp1[i] + hpA2 * v3;
                const float v2 = hp2[i] + hpA2 * hp1[i] + hpA3 * v3;
                hp1[i] = 2.0f * v1 - hp1[i];
                hp2[i] = 2.0f * v2 - hp2[i];
                const float damped = lp[i] - 1.41421356f * v1 - v2;
                const float v = damped + sFreeze * (raw - damped);

                sGain[i] += smoothFast * (tGain[i] - sGain[i]);
                d[i] = v;
                m[i] = v * (sGain[i] + sFreeze * (1.0f - sGain[i]));
            }

            // stereo taps
            const float wetL0 = 0.5f * (d[0] - d[2] + d[4] - d[6]);
            const float wetR0 = 0.5f * (d[1] - d[3] + d[5] - d[7]);

            // Hadamard feedback matrix (fast Walsh-Hadamard transform)
            for (int h = 1; h < numLines; h <<= 1)
                for (int i = 0; i < numLines; i += (h << 1))
                    for (int j = i; j < i + h; ++j)
                    {
                        const float a = m[j], b = m[j + h];
                        m[j] = a + b; m[j + h] = a - b;
                    }

            for (int i = 0; i < numLines; ++i)
            {
                float v = m[i] * hadamardScale + ((i & 1) ? xR : xL) * 0.5f;
                if (! (std::fabs (v) < 1.0e6f)) v = 0.0f;   // safety against NaN / runaway
                lines[i].write (v + 1.0e-20f);
            }

            // shimmer: pitch voices, gentle tone filter, safety limiter, cross-feed L <-> R
            if (shimmerOn)
            {
                shimHpL += shimHpCoef * (wetL0 - shimHpL);
                shimHpR += shimHpCoef * (wetR0 - shimHpR);
                const float sendL = wetL0 - shimHpL;
                const float sendR = wetR0 - shimHpR;

                float upL = sLevelA * voiceA[0].process (sendL, hann);
                float upR = sLevelA * voiceA[1].process (sendR, hann);
                if (voiceBOn)
                {
                    upL += sLevelB * voiceB[0].process (sendL, hann);
                    upR += sLevelB * voiceB[1].process (sendR, hann);
                }

                shimToneL += shimToneCoef * (upL - shimToneL);
                shimToneR += shimToneCoef * (upR - shimToneR);
                shimRetL = softLimit (shimToneR);
                shimRetR = softLimit (shimToneL);
            }

            // width
            const float mid  = 0.5f * (wetL0 + wetR0);
            const float side = 0.5f * (wetL0 - wetR0) * sWidth;

            const float wetGain = sWet * wetFade;
            outL[n] = dryL * sDry + (mid + side) * wetGain;
            outR[n] = dryR * sDry + (mid - side) * wetGain;
        }
    }

private:
    static inline float clamp (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    // sine approximation for the LFOs, phase 0..1
    static inline float fastSine (float phase)
    {
        const float x = 2.0f * phase - 1.0f;                 // -1..1
        const float y = 4.0f * x * (1.0f - std::fabs (x));   // parabola close to sin (pi x)
        return -(0.225f * (y * std::fabs (y) - y) + y);
    }

    // transparent below +/- 1.5, then a smooth knee that ends at +/- 2.5
    static inline float softLimit (float x)
    {
        const float a = std::fabs (x);
        if (a <= 1.5f) return x;
        const float y = 1.5f + std::tanh (a - 1.5f);
        return x < 0.0f ? -y : y;
    }

    void clearShimmer()
    {
        for (int c = 0; c < 2; ++c) { voiceA[c].clear(); voiceB[c].clear(); }
        shimRetL = shimRetR = shimToneL = shimToneR = shimHpL = shimHpR = 0.0f;
    }

    static constexpr float minSizeScale = 0.35f;
    static constexpr float maxSizeScale = 2.0f;
    static constexpr float maxShimmerLoopGain = 0.92f;

    double sr = 44100.0;
    Params params;

    DelayLine lines[numLines];
    float lenBase[numLines] {};
    float lp[numLines] {}, hp1[numLines] {}, hp2[numLines] {}, lfoPhase[numLines] {}, sGain[numLines] {};

    Allpass diffL[4], diffR[4];
    DelayLine preL, preR;

    PitchVoice voiceA[2], voiceB[2];
    float hann[PitchVoice::tableSize + 1] {};
    int   currentMode = -1;
    bool  shimmerWasOn = false, voiceBWasOn = false;
    float tLevelA = 1.0f, tLevelB = 0.0f;
    float shimRetL = 0, shimRetR = 0, shimToneL = 0, shimToneR = 0, shimHpL = 0, shimHpR = 0;
    float shimToneCoef = 0.5f, shimHpCoef = 0.01f;

    float smoothFast = 0.01f, smoothSlow = 0.001f;
    float sDry = 1, sWet = 0, sSize = 1, sPre = 1, sDepth = 0, sShim = 0, sWidth = 1, sFreeze = 0;
    float sLpCoef = 0.5f, sHpG = 0.01f, sLevelA = 1.0f, sLevelB = 0.0f;
    bool snapSmoothers = true;

    bool prepared = false;
    std::atomic<bool> flushRequested { false };
    bool  flushing = false;
    float wetFade = 1.0f, fadeStep = 0.002f;
};

} // namespace dreamy
