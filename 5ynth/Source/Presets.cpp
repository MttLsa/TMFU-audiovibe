#include "Presets.h"
#include "Params.h"
#include "BinaryData.h"

juce::String Preset::sections() const
{
    juce::StringArray s;
    for (auto& m : modules) s.add (params::moduleLetter (m.slot));
    return s.joinIntoString ("+");
}

const PresetLibrary& PresetLibrary::get() { static PresetLibrary lib; return lib; }

int PresetLibrary::modelIndex (const juce::String& name) const
{
    for (int i = 0; i < (int) modelList.size(); ++i) if (modelList[(size_t) i].name == name) return i;
    return -1;
}

PresetLibrary::PresetLibrary()
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::presets_json, BinaryData::presets_jsonSize));
    if (auto* g = json["genres"].getArray()) for (auto& x : *g) genres.add (x.toString());
    if (auto* d = json["decades"].getArray()) for (auto& x : *d) decades.add (x.toString());
    juce::StringArray names;
    if (auto* ms = json["models"].getArray())
        for (auto& m : *ms)
        {
            Model md; md.name = m["name"].toString(); md.skin = m["skin"].toString(); md.maker = m["maker"].toString();
            md.digital = m["kind"].toString() == "D"; md.year = (int) m["year"];
            modelList.push_back (md); names.add (md.name);
        }
    std::vector<int> defs;
    for (auto* n : { "Moog Minimoog Model D", "Yamaha DX7", "Roland Juno-106", "Korg M1", "Roland TB-303" })
        defs.push_back (juce::jmax (0, names.indexOf (n)));
    params::setModelNames (names, defs);
    if (auto* arr = json["presets"].getArray())
        for (auto& p : *arr)
        {
            Preset pr;
            pr.name = p["name"].toString(); pr.original = p["original"].toString(); pr.synth = p["synth"].toString();
            pr.genre = p["genre"].toString(); pr.song = p["song"].toString(); pr.artist = p["artist"].toString();
            pr.category = p["cat"].toString(); pr.notes = p["notes"].toString(); pr.decade = p["decade"].toString();
            pr.year = (int) p["year"];
            if (auto* mods = p["modules"].getArray())
                for (auto& m : *mods)
                {
                    PresetModule pm;
                    pm.slot = juce::jlimit (0, params::numModules - 1, (int) m["slot"]);
                    pm.model = m["model"].toString();
                    if (auto* obj = m["p"].getDynamicObject()) pm.values = obj->getProperties();
                    pr.modules.push_back (std::move (pm));
                }
            if (auto* obj = p["g"].getDynamicObject()) pr.globals = obj->getProperties();
            presets.push_back (std::move (pr));
        }
}
