#include "Presets.h"
#include "BinaryData.h"

const PresetLibrary& PresetLibrary::get() { static PresetLibrary lib; return lib; }

PresetLibrary::PresetLibrary()
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::presets_json, BinaryData::presets_jsonSize));
    for (auto& d : *json["decades"].getArray()) decades.add (d.toString());
    for (auto& g : *json["genres"].getArray()) genres.add (g.toString());
    for (auto& c : *json["categories"].getArray()) categories.add (c.toString());
    if (auto* arr = json["presets"].getArray())
        for (auto& p : *arr)
        {
            Preset pr;
            pr.name = p["name"].toString();
            pr.original = p["original"].toString();
            pr.synth = p["synth"].toString();
            pr.synthA = p["synthA"].toString();
            pr.synthD = p["synthD"].toString();
            pr.skinA = p["skinA"].toString();
            pr.skinD = p["skinD"].toString();
            pr.genre = p["genre"].toString();
            pr.decade = p["decade"].toString();
            pr.type = p["type"].toString();
            pr.category = p["cat"].toString();
            pr.reference = p["ref"].toString();
            pr.notes = p["notes"].toString();
            pr.year = (int) p["year"];
            pr.dance = (bool) p["dance"];
            if (auto* obj = p["p"].getDynamicObject())
                pr.values = obj->getProperties();
            presets.push_back (std::move (pr));
        }
}
