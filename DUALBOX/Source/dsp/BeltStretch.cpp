#include "BeltStretch.h"
#if defined (__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined (__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wsign-conversion"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wfloat-equal"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wunused-parameter"
 #pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
#include "../../third_party/signalsmith-stretch/signalsmith-stretch.h"
#if defined (__clang__)
 #pragma clang diagnostic pop
#elif defined (__GNUC__)
 #pragma GCC diagnostic pop
#endif

using namespace juce;

BeltStretch::BeltStretch (SampleLibrary& lib) : Thread ("Belt pitch engine"), library (lib) {}

BeltStretch::~BeltStretch() { stopThread (4000); }

void BeltStretch::setCorrectionFunction (CorrectionFn fn) { correction = std::move (fn); }

void BeltStretch::start() { if (correction && ! isThreadRunning()) startThread (Thread::Priority::low); }

SamplePtr BeltStretch::lookup (const SampleSet* set, int index, float semis) const
{
    auto t = std::atomic_load (&table);
    if (t == nullptr || t->set != set || index < 0 || index >= (int) t->items.size()) return nullptr;
    auto it = std::atomic_load (&t->items[(size_t) index]);
    if (it == nullptr || std::abs (it->semis - semis) > 0.005f) return nullptr;
    return it->smp;
}

SamplePtr BeltStretch::render (const Sample& s, float semis)
{
    using Stretch = signalsmith::stretch::SignalsmithStretch<float>;
    const int chans = s.stereo() ? 2 : 1;
    Stretch st;
    st.presetDefault (chans, (float) s.sourceRate);                        // tonal material: Groovulation's default preset
    st.setTransposeSemitones (semis, 8000.0f / (float) s.sourceRate);
    const int len = s.length();
    const int pad = 2 * (st.blockSamples() + st.intervalSamples());       // flushes the tail, satisfies the seek length
    const int total = len + pad;
    std::vector<std::vector<float>> in ((size_t) chans, std::vector<float> ((size_t) total, 0.0f)), out = in;
    std::copy (s.L.begin(), s.L.end(), in[0].begin());
    if (chans > 1) std::copy (s.R.begin(), s.R.end(), in[1].begin());
    std::vector<float*> ip ((size_t) chans), op ((size_t) chans);
    for (int c = 0; c < chans; ++c) { ip[(size_t) c] = in[(size_t) c].data(); op[(size_t) c] = out[(size_t) c].data(); }
    st.exact (ip.data(), total, op.data(), total);                       // same duration, latency compensated
    auto r = std::make_shared<Sample> (s);
    r->L.assign (out[0].begin(), out[0].begin() + len);
    if (chans > 1) r->R.assign (out[1].begin(), out[1].begin() + len);
    // short fade-out on the last 5 ms (the shifted tail is cut at the original length)
    const int f = jmin (len, (int) (s.sourceRate * 0.005));
    for (int i = 0; i < f; ++i)
    {
        const float g = (float) i / (float) jmax (1, f);
        r->L[(size_t) (len - 1 - i)] *= g;
        if (chans > 1) r->R[(size_t) (len - 1 - i)] *= g;
    }
    return r;
}

void BeltStretch::run()
{
    while (! threadShouldExit())
    {
        auto set = library.set (SampleLibrary::tie);
        auto t = std::atomic_load (&table);
        if (t == nullptr || t->set != set.get())
        {
            auto nt = std::make_shared<Table>();
            nt->set = set.get(); nt->keep = set;
            nt->items.resize ((size_t) set->size());
            std::atomic_store (&table, std::shared_ptr<const Table> (nt));
            t = nt;
        }
        bool didWork = false;
        for (int i = 0; i < set->size() && ! threadShouldExit(); ++i)
        {
            const float want = correction (*set->samples[(size_t) i]);
            auto cur = std::atomic_load (&t->items[(size_t) i]);
            if (std::isnan (want) || std::abs (want) < 0.005f)
            {
                if (cur != nullptr) std::atomic_store (&t->items[(size_t) i], std::shared_ptr<const Item>());
                continue;
            }
            if (cur != nullptr && std::abs (cur->semis - want) <= 0.005f) continue;
            busy.store (1);
            didWork = true;
            auto item = std::make_shared<Item>();
            item->semis = want;
            item->smp = render (*set->samples[(size_t) i], want);
            std::atomic_store (&t->items[(size_t) i], std::shared_ptr<const Item> (item));
            if (library.set (SampleLibrary::tie) != set) break;   // folder changed: start over
        }
        if (! didWork) { busy.store (0); wait (40); }
    }
}
