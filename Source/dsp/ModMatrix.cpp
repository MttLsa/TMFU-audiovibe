#include "ModMatrix.h"

using namespace juce;

void ModMatrix::init (AudioProcessorValueTreeState& apvts)
{
    state = &apvts;
    for (int sec = 0; sec < 2; ++sec)
    {
        auto& list = entries[(size_t) sec];
        list.clear();
        for (auto& t : params::lfoTargets (sec))
        {
            auto e = std::make_unique<Entry>();
            e->param = apvts.getParameter (t.id);
            e->raw = apvts.getRawParameterValue (t.id);
            jassert (e->param != nullptr && e->raw != nullptr);
            e->shadow.store (e->raw != nullptr ? e->raw->load() : 0.0f);
            byId[t.id] = e.get();
            list.push_back (std::move (e));
        }
        for (int i = 0; i < params::numLfos; ++i)
        {
            auto& l = lfos[(size_t) sec][(size_t) i];
            l.target   = apvts.getRawParameterValue (params::lfoId (sec, i, "target"));
            l.shape    = apvts.getRawParameterValue (params::lfoId (sec, i, "shape"));
            l.sync     = apvts.getRawParameterValue (params::lfoId (sec, i, "sync"));
            l.div      = apvts.getRawParameterValue (params::lfoId (sec, i, "div"));
            l.ms       = apvts.getRawParameterValue (params::lfoId (sec, i, "ms"));
            l.depth    = apvts.getRawParameterValue (params::lfoId (sec, i, "depth"));
            l.phaseOff = apvts.getRawParameterValue (params::lfoId (sec, i, "phase"));
            l.rng.setSeed ((int64) (sec * 10 + i + 1) * 7919);
            l.cur = l.rng.nextFloat() * 2.0f - 1.0f; l.next = l.rng.nextFloat() * 2.0f - 1.0f;
        }
    }
}

std::atomic<float>* ModMatrix::get (const String& id)
{
    auto it = byId.find (id);
    if (it != byId.end()) return &it->second->shadow;
    return state != nullptr ? state->getRawParameterValue (id) : nullptr;
}

float ModMatrix::offsetFor (const String& id) const
{
    auto it = byId.find (id);
    return it != byId.end() ? it->second->offset.load() : 0.0f;
}

float ModMatrix::shapeValue (int shape, double ph, float randCur, float randNext)
{
    ph -= std::floor (ph);
    switch (shape)
    {
        case 0:  return (float) std::sin (2.0 * MathConstants<double>::pi * ph);                          // sine
        case 1:  return (float) (ph < 0.25 ? 4.0 * ph : (ph < 0.75 ? 2.0 - 4.0 * ph : 4.0 * ph - 4.0));    // triangle
        case 2:  return (float) (2.0 * ph - 1.0);                                                           // saw up
        case 3:  return (float) (1.0 - 2.0 * ph);                                                           // saw down
        case 4:  return ph < 0.5 ? 1.0f : -1.0f;                                                            // square
        case 5:  return randCur;                                                                            // sample & hold
        default:                                                                                            // smooth random
        {
            const float w = 0.5f - 0.5f * (float) std::cos (MathConstants<double>::pi * ph);
            return randCur + (randNext - randCur) * w;
        }
    }
}

void ModMatrix::process (const BlockTime& bt, double sampleRate, int numSamples)
{
    const double blockSec = numSamples / jmax (1.0, sampleRate);
    for (int sec = 0; sec < 2; ++sec)
    {
        auto& list = entries[(size_t) sec];
        for (auto& e : list) e->acc = 0.0f;

        for (int i = 0; i < params::numLfos; ++i)
        {
            auto& l = lfos[(size_t) sec][(size_t) i];
            const int target = (int) l.target->load() - 1;                       // 0 = "--"
            const bool sync = l.sync->load() > 0.5f;
            const auto& divs = params::lfoDivs();
            const double beats = divs[(size_t) jlimit (0, (int) divs.size() - 1, (int) l.div->load())].beats;
            const double off = l.phaseOff->load() / 360.0;

            // phase in cycles: in SYNC mode locked to the song position while playing (beats), otherwise free running (ms)
            double cycles;
            if (sync && bt.playing)
            {
                cycles = bt.ppqStart / beats + off;
                l.phase = cycles - off;
            }
            else
            {
                const double hz = sync ? (bt.bpm / 60.0) / beats : 1000.0 / jmax (1.0f, l.ms->load());
                l.phase += hz * blockSec;
                if (l.phase > 1.0e6) l.phase -= std::floor (l.phase);
                cycles = l.phase + off;
            }
            const long long cyc = (long long) std::floor (cycles);
            if (cyc != l.lastCycle)
            {
                if (l.lastCycle >= 0) { l.cur = l.next; l.next = l.rng.nextFloat() * 2.0f - 1.0f; }   // new random value each cycle
                l.lastCycle = cyc;
            }
            const float val = shapeValue ((int) l.shape->load(), cycles, l.cur, l.next);
            l.out.store (val);
            l.uiPhase.store ((float) (cycles - std::floor (cycles)));
            if (target >= 0 && target < (int) list.size())
                list[(size_t) target]->acc += val * l.depth->load() * 0.01f * 0.5f;    // depth 100 % = +-half of the range
        }

        for (auto& e : list)
        {
            if (e->raw == nullptr) continue;
            const float raw = e->raw->load();
            if (std::abs (e->acc) < 1.0e-6f)
            {
                e->shadow.store (raw);
                if (e->wasModulated) { e->offset.store (0.0f); e->wasModulated = false; }
                continue;
            }
            const float n = jlimit (0.0f, 1.0f, e->param->convertTo0to1 (raw) + e->acc);
            e->shadow.store (e->param->convertFrom0to1 (n));
            e->offset.store (e->acc);
            e->wasModulated = true;
        }
    }
}
