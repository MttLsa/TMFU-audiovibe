#include "Presets.h"
#include "BinaryData.h"

const PresetLibrary& PresetLibrary::get() { static PresetLibrary lib; return lib; }

PresetLibrary::PresetLibrary()
{
    const auto json = juce::JSON::parse (juce::String::fromUTF8 (BinaryData::presets_json, BinaryData::presets_jsonSize));
    if (auto* cats = json["categories"].getArray())
        for (auto& c : *cats) categories.add (c.toString());
    if (auto* arr = json["presets"].getArray())
        for (auto& p : *arr)
        {
            Preset pr;
            pr.name = p["name"].toString();
            pr.category = p["cat"].toString();
            pr.notes = p["notes"].toString();
            if (auto* obj = p["p"].getDynamicObject()) pr.values = obj->getProperties();
            pr.lanes = p["lanes"];
            pr.lfoShapes = p["lfo"];
            presets.push_back (std::move (pr));
        }
}
