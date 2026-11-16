// D.A AURA DSP core - plain C++ (no JUCE dependency)
// Signal flow:
//   in -> pre-delay -> (+ shimmer feedback) -> 4x allpass diffusion
//      -> 8-line modulated FDN (Hadamard matrix, damping in the loop)
//      -> width -> equal-power dry/wet mix
//   FDN output -> octave-up pitch shifter -> soft clip -> back into the input
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace aura
{

class DelayLine
{
public:
    void init (int minLength)
    {
        int size = 16;
        while (size < minLength + 4) size <<= 1;
        buf.assign ((size_t) size, 0.0f);
        mask = size - 1;
        w = 0;
    }
    void clear()                       { std::fill (buf.begin(), buf.end(), 0.0f); }
    inline void write (float x)        { buf[(size_t) w] = x; w = (w + 1) & mask; }
    // delay in samples, measured back from the most recently written sample (d >= 1)
    inline float read (int d) const    { return buf[(size_t) ((w - d) & mask)]; }
    inline float readFrac (float d) const
    {
        const int   di = (int) d;
        const float fr = d - (float) di;
        const float a = buf[(size_t) ((w - di) & mask)];
        const float b = buf[(size_t) ((w - di - 1) & mask)];
        return a + fr * (b - a);
    }
private:
    std::vector<float> buf;
    int mask = 0, w = 0;
};

class Allpass
{
public:
    void init (int lengthSamples)      { len = std::max (1, lengthSamples); line.init (len); }
    void clear()                       { line.clear(); }
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

// Simple two-tap crossfading delay-line pitch shifter (one octave up)
class OctaveUp
{
public:
    void init (double sampleRate)
    {
        window = (float) (0.09 * sampleRate);
        line.init ((int) window + 8);
        inc = 1.0f / window;           // (ratio - 1) / window, ratio = 2
        phase = 0.0f;
    }
    void clear()                       { line.clear(); phase = 0.0f; }
    inline float process (float x)
    {
        line.write (x);
        float p2 = phase + 0.5f;
        if (p2 >= 1.0f) p2 -= 1.0f;
        const float twoPi = 6.283185307179586f;
        const float g1 = 0.5f - 0.5f * std::cos (twoPi * phase);
        const float g2 = 1.0f - g1;
        const float y = g1 * line.readFrac (1.0f + window * (1.0f - phase))
                      + g2 * line.readFrac (1.0f + window * (1.0f - p2));
        phase += inc;
        if (phase >= 1.0f) phase -= 1.0f;
        return y;
    }
private:
    DelayLine line;
    float window = 4000.0f, inc = 0.0f, phase = 0.0f;
};

class Reverb
{
public:
    static constexpr int numLines = 8;

    struct Params
    {
        float mix       = 0.4f;    // 0..1
        float size      = 0.6f;    // 0..1
        float decay     = 6.0f;    // seconds (RT60)
        float preDelay  = 20.0f;   // ms (0..500)
        float highCut   = 7000.0f; // Hz, damping inside the loop
        float lowCut    = 120.0f;  // Hz, inside the loop
        float modDepth  = 0.4f;    // 0..1
        float modRate   = 0.5f;    // Hz
        float shimmer   = 0.25f;   // 0..1
        float width     = 1.0f;    // 0..1.5
        bool  freeze    = false;
    };

    void prepare (double sampleRate)
    {
        sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
        const double k = sr / 48000.0;

        static const int baseLen[numLines] = { 1487, 1699, 1933, 2213, 2539, 2903, 3271, 3697 };
        for (int i = 0; i < numLines; ++i)
        {
            lenBase[i] = (float) (baseLen[i] * k);
            lines[i].init ((int) (lenBase[i] * maxSizeScale + 64.0 * k) + 8);
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
        shiftL.init (sr);
        shiftR.init (sr);

        smoothFast = 1.0f - std::exp (-1.0f / (float) (0.02 * sr));
        smoothSlow = 1.0f - std::exp (-1.0f / (float) (0.25 * sr));
        shimLpCoef = 1.0f - std::exp (-6.2831853f * 6000.0f / (float) sr);

        reset();
        snapSmoothers = true;
    }

    void reset()
    {
        for (int i = 0; i < numLines; ++i) { lines[i].clear(); lp[i] = 0.0f; hp[i] = 0.0f; }
        for (int i = 0; i < 4; ++i)        { diffL[i].clear(); diffR[i].clear(); }
        preL.clear(); preR.clear(); shiftL.clear(); shiftR.clear();
        shimFbL = shimFbR = shimLpL = shimLpR = 0.0f;
    }

    void setParams (const Params& p) { params = p; }

    // In-place safe (inL may equal outL etc.)
    void process (const float* inL, const float* inR, float* outL, float* outR, int numSamples)
    {
        const float fsr = (float) sr;

        // ---- per-block targets -------------------------------------------------
        const float tMix     = clamp (params.mix, 0.0f, 1.0f);
        const float tDry     = std::cos (tMix * 1.5707963f);
        const float tWet     = std::sin (tMix * 1.5707963f);
        const float tSize    = minSizeScale + (maxSizeScale - minSizeScale) * clamp (params.size, 0.0f, 1.0f);
        const float tPre     = 1.0f + clamp (params.preDelay, 0.0f, 500.0f) * 0.001f * fsr;
        const float tDepth   = clamp (params.modDepth, 0.0f, 1.0f) * 16.0f * fsr / 48000.0f;
        const float tShim    = params.freeze ? 0.0f : clamp (params.shimmer, 0.0f, 1.0f) * 0.55f;
        const float tWidth   = clamp (params.width, 0.0f, 1.5f);
        const float tInGain  = params.freeze ? 0.0f : 1.0f;
        const bool  frozen   = params.freeze;

        const float decay    = clamp (params.decay, 0.1f, 60.0f);
        const float lpCoef   = 1.0f - std::exp (-6.2831853f * clamp (params.highCut, 200.0f, 0.45f * fsr) / fsr);
        const float hpCoef   = 1.0f - std::exp (-6.2831853f * clamp (params.lowCut, 5.0f, 2000.0f) / fsr);

        float gain[numLines], lfoInc[numLines];
        const float rate = clamp (params.modRate, 0.01f, 10.0f);
        for (int i = 0; i < numLines; ++i)
        {
            const float lenSec = lenBase[i] * tSize / fsr;
            gain[i]   = frozen ? 1.0f : std::pow (10.0f, -3.0f * lenSec / decay);
            lfoInc[i] = rate * (1.0f + 0.083f * (float) i) / fsr;
        }

        if (snapSmoothers)
        {
            sDry = tDry; sWet = tWet; sSize = tSize; sPre = tPre; sDepth = tDepth;
            sShim = tShim; sWidth = tWidth; sInGain = tInGain;
            snapSmoothers = false;
        }

        const float diffG  = 0.62f;
        const float hadamardScale = 0.35355339f; // 1/sqrt(8)
        const float twoPi  = 6.283185307179586f;

        for (int n = 0; n < numSamples; ++n)
        {
            sDry    += smoothFast * (tDry    - sDry);
            sWet    += smoothFast * (tWet    - sWet);
            sWidth  += smoothFast * (tWidth  - sWidth);
            sShim   += smoothFast * (tShim   - sShim);
            sInGain += smoothFast * (tInGain - sInGain);
            sDepth  += smoothFast * (tDepth  - sDepth);
            sSize   += smoothSlow * (tSize   - sSize);
            sPre    += smoothSlow * (tPre    - sPre);

            const float dryL = inL[n];
            const float dryR = inR[n];

            // pre-delay
            preL.write (dryL);
            preR.write (dryR);
            float xL = preL.readFrac (sPre) * sInGain + shimFbL * sShim;
            float xR = preR.readFrac (sPre) * sInGain + shimFbR * sShim;

            // input diffusion
            for (int i = 0; i < 4; ++i)
            {
                xL = diffL[i].process (xL, diffG);
                xR = diffR[i].process (xR, diffG);
            }

            // read the modulated delay lines, filter and attenuate
            float d[numLines];
            for (int i = 0; i < numLines; ++i)
            {
                lfoPhase[i] += lfoInc[i];
                if (lfoPhase[i] >= 1.0f) lfoPhase[i] -= 1.0f;
                const float mod = sDepth * (1.0f + std::sin (twoPi * lfoPhase[i]));
                float v = lines[i].readFrac (2.0f + lenBase[i] * sSize + mod);

                if (! frozen)
                {
                    lp[i] += lpCoef * (v - lp[i]);
                    v = lp[i];
                    hp[i] += hpCoef * (v - hp[i]);
                    v -= hp[i];
                }
                d[i] = v;
            }

            // stereo taps
            const float wetL0 = 0.5f * (d[0] - d[2] + d[4] - d[6]);
            const float wetR0 = 0.5f * (d[1] - d[3] + d[5] - d[7]);

            // Hadamard feedback matrix (fast Walsh-Hadamard transform)
            float m[numLines];
            for (int i = 0; i < numLines; ++i) m[i] = d[i] * gain[i];
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

            // shimmer: octave up, darken a little, soft clip, cross-feed L<->R
            shimLpL += shimLpCoef * (shiftL.process (wetL0) - shimLpL);
            shimLpR += shimLpCoef * (shiftR.process (wetR0) - shimLpR);
            shimFbL = std::tanh (shimLpR);
            shimFbR = std::tanh (shimLpL);

            // width
            const float mid  = 0.5f * (wetL0 + wetR0);
            const float side = 0.5f * (wetL0 - wetR0) * sWidth;

            outL[n] = dryL * sDry + (mid + side) * sWet;
            outR[n] = dryR * sDry + (mid - side) * sWet;
        }
    }

private:
    static inline float clamp (float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

    static constexpr float minSizeScale = 0.35f;
    static constexpr float maxSizeScale = 2.0f;

    double sr = 44100.0;
    Params params;

    DelayLine lines[numLines];
    float lenBase[numLines] {};
    float lp[numLines] {}, hp[numLines] {}, lfoPhase[numLines] {};

    Allpass diffL[4], diffR[4];
    DelayLine preL, preR;
    OctaveUp shiftL, shiftR;
    float shimFbL = 0, shimFbR = 0, shimLpL = 0, shimLpR = 0, shimLpCoef = 0.5f;

    float smoothFast = 0.01f, smoothSlow = 0.001f;
    float sDry = 1, sWet = 0, sSize = 1, sPre = 1, sDepth = 0, sShim = 0, sWidth = 1, sInGain = 1;
    bool snapSmoothers = true;
};

} // namespace aura
