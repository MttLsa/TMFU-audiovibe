#pragma once
#include <vector>
#include <cmath>
#include <algorithm>

// =====================================================================================================
//  Band-limited wavetable bank for Section D. 12 tables x 8 frames, each frame stored as 11 mip levels
//  (1024, 512, ... 1 harmonics) of 2048 samples. Built once (inverse FFT), shared by all instances.
// =====================================================================================================
class WavetableBank
{
public:
    static constexpr int numTables = 12, numFrames = 8, size = 2048, numLevels = 11;
    static const WavetableBank& get();

    // phase 0..1, inc = f / sr, pos 0..1 (frame morph)
    inline float read (int table, float pos, float phase, float inc) const
    {
        const float maxH = 0.5f / std::max (1.0e-6f, inc);
        int lvl = 0;
        while (lvl < numLevels - 1 && (float) (1024 >> lvl) > maxH) ++lvl;
        const float fp = std::clamp (pos, 0.0f, 1.0f) * (numFrames - 1);
        const int f0 = std::min ((int) fp, numFrames - 2);
        const float ff = fp - (float) f0;
        const float x = phase * (float) size;
        const int i = ((int) x) & (size - 1);
        const float fr = x - std::floor (x);
        const float* a = frame (table, f0, lvl);
        const float* b = frame (table, f0 + 1, lvl);
        const float va = a[i] + fr * (a[i + 1] - a[i]);
        const float vb = b[i] + fr * (b[i + 1] - b[i]);
        return va + ff * (vb - va);
    }

private:
    WavetableBank();
    const float* frame (int t, int f, int l) const { return data.data() + ((size_t) ((t * numFrames + f) * numLevels + l)) * (size + 1); }
    std::vector<float> data;
};
