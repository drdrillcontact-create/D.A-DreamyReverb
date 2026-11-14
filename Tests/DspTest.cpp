// Self test of the reverb algorithm (plain C++, no JUCE). Returns 0 when everything passes.
#include "DreamyReverbDSP.h"
#include "Presets.h"

#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

namespace
{
    int failures = 0;

    void check (bool ok, const char* what)
    {
        std::printf ("%s  %s\n", ok ? "ok  " : "FAIL", what);
        if (! ok) ++failures;
    }

    dreamy::Reverb::Params paramsFor (const dreamy::Preset& p)
    {
        dreamy::Reverb::Params r;
        r.mix = p.mix;            r.size = p.size;          r.decay = p.decay;
        r.preDelay = p.preDelay;  r.highCut = p.highCut;    r.lowCut = p.lowCut;
        r.modDepth = p.modDepth;  r.modRate = p.modRate;    r.shimmer = p.shimmer;
        r.width = p.width;        r.shimmerMode = p.interval;
        return r;
    }

    // level of one frequency (Goertzel)
    double toneLevel (const std::vector<float>& x, size_t from, size_t to, double freq, double sr)
    {
        const double w = 2.0 * 3.14159265358979 * freq / sr, c = 2.0 * std::cos (w);
        double s1 = 0.0, s2 = 0.0;
        for (size_t i = from; i < to; ++i) { const double s0 = x[i] + c * s1 - s2; s2 = s1; s1 = s0; }
        return std::sqrt (std::max (0.0, s1 * s1 + s2 * s2 - c * s1 * s2)) / (double) (to - from) * 2.0;
    }

    struct Result { bool finite = true; double peak = 0.0, tailRms = 0.0; };

    // feeds "seconds" of noise bursts and tones, then silence, and reports what comes out
    Result run (dreamy::Reverb& rv, const dreamy::Reverb::Params& p, double sr, int block, double seconds, double silence,
                std::vector<float>* keepLeft = nullptr, double toneHz = 0.0)
    {
        std::mt19937 rng (7);
        std::uniform_real_distribution<float> noise (-1.0f, 1.0f);
        std::vector<float> l ((size_t) block), r ((size_t) block);
        Result res;
        const long total = (long) ((seconds + silence) * sr), active = (long) (seconds * sr);
        double tailSum = 0.0; long tailCount = 0;

        rv.setParams (p);
        for (long pos = 0; pos < total; pos += block)
        {
            for (int i = 0; i < block; ++i)
            {
                const long n = pos + i;
                float s = 0.0f;
                if (n < active)
                    s = toneHz > 0.0 ? 0.25f * (float) std::sin (2.0 * 3.14159265358979 * toneHz * (double) n / sr)
                                     : ((n / (long) (sr * 0.25)) % 2 == 0 ? 0.5f * noise (rng) : 0.0f);
                l[(size_t) i] = s;
                r[(size_t) i] = s;
            }

            rv.process (l.data(), r.data(), l.data(), r.data(), block);

            for (int i = 0; i < block; ++i)
            {
                const float a = l[(size_t) i], b = r[(size_t) i];
                if (! std::isfinite (a) || ! std::isfinite (b)) res.finite = false;
                res.peak = std::max (res.peak, (double) std::max (std::fabs (a), std::fabs (b)));
                if (pos + i >= total - (long) (0.1 * sr)) { tailSum += (double) a * a; ++tailCount; }
                if (keepLeft != nullptr) keepLeft->push_back (a);
            }
        }

        res.tailRms = std::sqrt (tailSum / (double) std::max (1L, tailCount));
        return res;
    }
}

int main()
{
    char text[256];

    // ---- preset table ---------------------------------------------------------------------
    check (dreamy::numPresets == 50, "there are 50 presets");
    bool rangesOk = true, namesOk = true;
    for (int i = 0; i < dreamy::numPresets; ++i)
    {
        const auto& p = dreamy::factoryPresets[i];
        rangesOk = rangesOk && p.name != nullptr && p.category != nullptr
                && p.mix >= 0.0f && p.mix <= 1.0f && p.size >= 0.0f && p.size <= 1.0f
                && p.decay >= 0.3f && p.decay <= 30.0f && p.preDelay >= 0.0f && p.preDelay <= 500.0f
                && p.highCut >= 1000.0f && p.highCut <= 20000.0f && p.lowCut >= 20.0f && p.lowCut <= 1000.0f
                && p.modDepth >= 0.0f && p.modDepth <= 1.0f && p.modRate >= 0.05f && p.modRate <= 3.0f
                && p.shimmer >= 0.0f && p.shimmer <= 1.0f && p.width >= 0.0f && p.width <= 1.5f
                && p.interval >= 0 && p.interval < dreamy::numShimmerIntervals;
        for (int j = 0; j < i; ++j)
            namesOk = namesOk && std::strcmp (p.name, dreamy::factoryPresets[j].name) != 0;
    }
    check (rangesOk, "every preset value is inside its parameter range");
    check (namesOk, "every preset has its own name");

    // ---- every preset, several sample rates and block sizes --------------------------------
    const double rates[] = { 44100.0, 48000.0, 96000.0 };
    const int blocks[] = { 1, 64, 480, 4096 };
    bool allFinite = true, allBounded = true;
    double worstPeak = 0.0;
    for (int i = 0; i < dreamy::numPresets; ++i)
    {
        dreamy::Reverb rv;
        const double sr = rates[i % 3];
        rv.prepare (sr);
        const auto res = run (rv, paramsFor (dreamy::factoryPresets[i]), sr, blocks[i % 4], 1.5, 0.5);
        allFinite = allFinite && res.finite;
        allBounded = allBounded && res.peak < 4.0;
        worstPeak = std::max (worstPeak, res.peak);
    }
    check (allFinite, "all presets: output is finite");
    std::snprintf (text, sizeof (text), "all presets: output stays bounded (highest peak %.2f for a 0.5 peak input)", worstPeak);
    check (allBounded, text);

    // ---- silence in, silence out ---------------------------------------------------------------
    {
        dreamy::Reverb rv; rv.prepare (48000.0);
        dreamy::Reverb::Params p; p.shimmer = 1.0f; p.decay = 30.0f;
        const auto res = run (rv, p, 48000.0, 256, 0.0, 2.0);
        check (res.finite && res.peak < 1.0e-6, "silence in gives silence out");
    }

    // ---- the tail decays ----------------------------------------------------------------------
    {
        dreamy::Reverb rv; rv.prepare (48000.0);
        dreamy::Reverb::Params p; p.mix = 1.0f; p.decay = 1.0f; p.shimmer = 0.5f;
        const auto res = run (rv, p, 48000.0, 256, 0.5, 6.0);
        std::snprintf (text, sizeof (text), "a 1 s decay is silent after 6 s (tail rms %.2e)", res.tailRms);
        check (res.finite && res.tailRms < 1.0e-4, text);
    }

    // ---- extreme settings stay stable --------------------------------------------------------------
    for (int mode = 0; mode < dreamy::Reverb::numShimmerModes; ++mode)
    {
        dreamy::Reverb rv; rv.prepare (44100.0);
        dreamy::Reverb::Params p;
        p.mix = 1.0f; p.decay = 30.0f; p.size = 1.0f; p.shimmer = 1.0f; p.shimmerMode = mode;
        p.highCut = 20000.0f; p.lowCut = 20.0f; p.modDepth = 1.0f; p.modRate = 3.0f; p.width = 1.5f;
        const auto res = run (rv, p, 44100.0, 512, 20.0, 1.0);
        std::snprintf (text, sizeof (text), "interval %d: 30 s decay with full shimmer stays stable (peak %.2f)", mode, res.peak);
        check (res.finite && res.peak < 12.0, text);
    }

    // ---- the shimmer adds the octave ------------------------------------------------------------------
    {
        const double sr = 48000.0;
        double octave[2] = {};
        for (int pass = 0; pass < 2; ++pass)
        {
            dreamy::Reverb rv; rv.prepare (sr);
            dreamy::Reverb::Params p; p.mix = 1.0f; p.decay = 5.0f; p.modDepth = 0.0f; p.shimmer = pass == 0 ? 0.0f : 0.7f;
            std::vector<float> out;
            run (rv, p, sr, 256, 6.0, 0.0, &out, 440.0);
            double sum = 0.0;                       // average over a few neighbouring bins (the octave is slightly detuned)
            for (double f = 876.0; f <= 884.0; f += 1.0)
                sum += toneLevel (out, (size_t) (3.0 * sr), out.size(), f, sr);
            octave[pass] = sum;
        }
        std::snprintf (text, sizeof (text), "shimmer creates the octave above a 440 Hz tone (%.1f dB above the level without shimmer)",
                       20.0 * std::log10 ((octave[1] + 1.0e-12) / (octave[0] + 1.0e-12)));
        check (octave[1] > octave[0] * 30.0, text);
    }

    // ---- freeze holds the tail, releasing it lets it fade --------------------------------------------
    {
        const double sr = 48000.0;
        dreamy::Reverb rv; rv.prepare (sr);
        dreamy::Reverb::Params p; p.mix = 1.0f; p.decay = 2.0f;
        run (rv, p, sr, 256, 1.0, 0.0);
        p.freeze = true;
        const auto early = run (rv, p, sr, 256, 0.0, 1.0);
        const auto late  = run (rv, p, sr, 256, 0.0, 20.0);
        std::snprintf (text, sizeof (text), "freeze holds the tail for 20 s (rms %.4f -> %.4f)", early.tailRms, late.tailRms);
        check (late.finite && late.tailRms > early.tailRms * 0.5 && late.tailRms < early.tailRms * 1.5, text);
        p.freeze = false;
        const auto released = run (rv, p, sr, 256, 0.0, 8.0);
        check (released.tailRms < late.tailRms * 0.01, "releasing freeze lets the tail fade out");
    }

    // ---- loading a preset empties the reverb without a click -----------------------------------------
    {
        const double sr = 48000.0;
        dreamy::Reverb rv; rv.prepare (sr);
        dreamy::Reverb::Params p; p.mix = 1.0f; p.decay = 20.0f; p.size = 0.2f;
        run (rv, p, sr, 256, 2.0, 0.0);

        p.size = 1.0f; p.decay = 3.0f;       // a very different "preset"
        rv.flush();
        std::vector<float> out;
        const auto res = run (rv, p, sr, 256, 0.0, 0.5, &out);

        float biggestStep = 0.0f;
        for (size_t i = 1; i < out.size(); ++i)
            biggestStep = std::max (biggestStep, std::fabs (out[i] - out[i - 1]));

        std::snprintf (text, sizeof (text), "flush: the old tail is gone after the fade (rms %.2e, biggest step %.3f)", res.tailRms, biggestStep);
        check (res.finite && res.tailRms < 1.0e-5 && biggestStep < 0.2, text);
    }

    // ---- random automation of every parameter --------------------------------------------------------
    {
        const double sr = 48000.0;
        dreamy::Reverb rv; rv.prepare (sr);
        std::mt19937 rng (99);
        std::uniform_real_distribution<float> u (0.0f, 1.0f);
        std::vector<float> l (2048), r (2048);
        bool finite = true; double peak = 0.0;
        dreamy::Reverb::Params p;
        for (long done = 0; done < (long) (30.0 * sr);)
        {
            const int n = 1 + (int) (u (rng) * 2047.0f);
            p.mix = u (rng); p.size = u (rng); p.decay = 0.3f + 29.7f * u (rng) * u (rng); p.preDelay = 500.0f * u (rng);
            p.highCut = 1000.0f + 19000.0f * u (rng); p.lowCut = 20.0f + 980.0f * u (rng); p.modDepth = u (rng);
            p.modRate = 0.05f + 2.95f * u (rng); p.shimmer = u (rng); p.width = 1.5f * u (rng);
            p.shimmerMode = (int) (u (rng) * 3.99f); p.freeze = u (rng) < 0.1f;
            if (u (rng) < 0.05f) rv.flush();
            for (int i = 0; i < n; ++i) { l[(size_t) i] = u (rng) - 0.5f; r[(size_t) i] = u (rng) - 0.5f; }
            rv.setParams (p);
            rv.process (l.data(), r.data(), l.data(), r.data(), n);
            for (int i = 0; i < n; ++i)
            {
                if (! std::isfinite (l[(size_t) i]) || ! std::isfinite (r[(size_t) i])) finite = false;
                peak = std::max (peak, (double) std::max (std::fabs (l[(size_t) i]), std::fabs (r[(size_t) i])));
            }
            done += n;
        }
        std::snprintf (text, sizeof (text), "random automation of all parameters stays stable (peak %.2f)", peak);
        check (finite && peak < 12.0, text);
    }

    std::printf ("\n%s (%d failure%s)\n", failures == 0 ? "DSP TEST PASSED" : "DSP TEST FAILED", failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
