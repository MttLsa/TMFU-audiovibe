#include "Presets.h"
#include "PluginProcessor.h"
#include <BinaryData.h>

using namespace juce;
using namespace params;

namespace
{
    void setNatural (AudioProcessorValueTreeState& s, const String& id, float value)
    {
        if (auto* prm = s.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (value)));
            prm->endChangeGesture();
        }
    }

    // parameters that a preset never touches: module on/off (incl. Pulp), pattern selection / chain, scale settings
    bool keepOnPresetLoad (const String& id)
    {
        return id.endsWith ("_on") || id.startsWith ("b_chain") || id == params::bPattern || id.contains ("scale");
    }

    std::vector<PresetLibrary::Item> parse (const char* data, int size)
    {
        std::vector<PresetLibrary::Item> out;
        auto v = JSON::parse (String::fromUTF8 (data, size));
        if (auto* arr = v.getProperty ("presets", var()).getArray())
            for (auto& p : *arr)
                out.push_back ({ p.getProperty ("name", "").toString(), p.getProperty ("genre", "").toString(),
                                 (int) p.getProperty ("bpm", 120), p });
        return out;
    }
}

PresetLibrary::PresetLibrary()
{
    a = parse (BinaryData::presets_A_json, BinaryData::presets_A_jsonSize);
    b = parse (BinaryData::presets_B_json, BinaryData::presets_B_jsonSize);
}

const PresetLibrary& PresetLibrary::get()
{
    static PresetLibrary lib;
    return lib;
}

StringArray PresetLibrary::genres() const
{
    StringArray g;
    for (auto& it : a) g.addIfNotAlreadyThere (it.genre);
    return g;
}

void PresetLibrary::applyA (TwinboxProcessor& p, const Item& it, int slot, bool kit, bool pattern, bool tempo)
{
    auto& s = p.apvts;
    const auto& d = it.data;
    if (kit)
    {
        if (auto* chs = d.getProperty ("kit", var()).getArray())
            for (int ch = 0; ch < jmin (numMicroChannels, chs->size()); ++ch)
                if (auto* obj = (*chs)[ch].getDynamicObject())
                    for (auto& prop : obj->getProperties())
                        setNatural (s, params::a (ch, prop.name.toString().toRawUTF8()), (float) prop.value);
        if (auto* names = d.getProperty ("names", var()).getArray())
            for (int ch = 0; ch < jmin (numMicroChannels, names->size()); ++ch) p.setChannelName (ch, (*names)[ch].toString());
        setNatural (s, aSwing, (float) d.getProperty ("swing", 0));
        setNatural (s, aFillRate, (float) (int) d.getProperty ("fillrate", 3));
    }
    if (pattern)
    {
        auto& P = p.micro.pattern (slot);
        P.clear();
        P.length.store (jlimit (1, numSteps, (int) d.getProperty ("len", 16)));
        if (auto* rows = d.getProperty ("steps", var()).getArray())
            for (int ch = 0; ch < jmin (numMicroChannels, rows->size()); ++ch)
            {
                const String row = (*rows)[ch].toString();
                for (int st = 0; st < jmin (numSteps, row.length()); ++st)
                    P.steps[(size_t) ch][(size_t) st].store ((uint8_t) jlimit (0, 7, (int) (row[st] - '0')));
            }
    }
    if (tempo) setNatural (s, gTempo, (float) it.bpm);
}

void PresetLibrary::applyB (TwinboxProcessor& p, const Item& it, bool kit, bool patterns, bool tempo)
{
    auto& s = p.apvts;
    const auto& d = it.data;
    if (kit)
    {
        // reset every voice/FX parameter of Section B to its default, then apply the preset's values
        for (auto* prm : p.getParameters())
            if (auto* rp = dynamic_cast<RangedAudioParameter*> (prm))
                if (rp->getParameterID().startsWith ("b_") && ! keepOnPresetLoad (rp->getParameterID()))
                    rp->setValueNotifyingHost (rp->getDefaultValue());
        if (auto* obj = d.getProperty ("params", var()).getDynamicObject())
            for (auto& prop : obj->getProperties())
                if (! keepOnPresetLoad (prop.name.toString()))
                    setNatural (s, prop.name.toString(), (float) prop.value);
        // samples are referenced by name inside the factory "Twinbox <Module>" folders
        if (auto* smp = d.getProperty ("samples", var()).getDynamicObject())
        {
            static const char* mods[] { "Kick", "Snare", "Hat", "Tie", "Pod" };
            for (int m = 0; m < SampleLibrary::numModules; ++m)
            {
                const String name = smp->getProperty (mods[m]).toString();
                if (name.isEmpty()) continue;
                const auto names = SampleLibrary::namesIn (SampleLibrary::folderFor (m, SampleLibrary::twinbox));
                const int idx = names.indexOf (name);
                if (idx < 0) continue;
                if (p.library.source (m) != SampleLibrary::twinbox) p.library.setSource (m, SampleLibrary::twinbox);
                setNatural (s, params::b (m, lv::wav), (float) idx);
            }
        }
    }
    if (patterns)
        if (auto* pats = d.getProperty ("patterns", var()).getArray())
            for (int pi = 0; pi < jmin (LimeEngine::NP, pats->size()); ++pi)
            {
                const auto& pd = (*pats)[pi];
                auto& P = p.lime.pattern (pi);
                P.clear();
                if (auto* trig = pd.getProperty ("trig", var()).getArray())
                    for (int l = 0; l < jmin ((int) LimeEngine::numTrigLanes, trig->size()); ++l)
                    {
                        const String row = (*trig)[l].toString();
                        for (int st = 0; st < jmin (numLimeSteps, row.length()); ++st)
                            P.trig[(size_t) l][(size_t) st].store ((uint8_t) jlimit (0, 2, (int) (row[st] - '0')));
                    }
                const char* lanes[] { "tie", "pod", "mod" };
                for (int l = 0; l < 3; ++l)
                    if (auto* vals = pd.getProperty (lanes[l], var()).getArray())
                        for (int st = 0; st < jmin (numLimeSteps, vals->size()); ++st)
                            P.value[(size_t) l][(size_t) st].store (jlimit (0.0f, 1.0f, (float) (*vals)[st]));
            }
    if (tempo) setNatural (s, gTempo, (float) it.bpm);
}
