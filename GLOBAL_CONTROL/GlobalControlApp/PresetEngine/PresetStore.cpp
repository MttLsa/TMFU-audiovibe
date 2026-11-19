#include "PresetStore.h"

namespace gc {

static const char* kExt = ".gcpreset";

PresetStore::PresetStore (juce::File r) : root (std::move (r)) {}

juce::File PresetStore::defaultRoot()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
#if JUCE_MAC
        .getChildFile ("Application Support")
#endif
        .getChildFile ("GLOBAL CONTROL");
}

juce::String PresetStore::sanitize (const juce::String& name)
{
    auto s = name.trim().removeCharacters ("/\\:*?\"<>|").substring (0, 80);
    return s.isEmpty() ? juce::String ("Untitled") : s;
}

juce::File PresetStore::dirFor (PresetScope scope, const juce::String& projectId) const
{
    if (scope == PresetScope::Global) return root.getChildFile ("Global");
    return root.getChildFile ("Projects").getChildFile (sanitize (projectId.isEmpty() ? "unknown-project" : projectId));
}

juce::File PresetStore::fileFor (PresetScope scope, const juce::String& projectId, const juce::String& name) const
{
    return dirFor (scope, projectId).getChildFile (sanitize (name) + kExt);
}

bool PresetStore::exists (PresetScope scope, const juce::String& projectId, const juce::String& name) const
{
    return fileFor (scope, projectId, name).existsAsFile();
}

juce::Array<PresetInfo> PresetStore::list (PresetScope scope, const juce::String& projectId) const
{
    juce::Array<PresetInfo> out;
    for (const auto& f : dirFor (scope, projectId).findChildFiles (juce::File::findFiles, false, juce::String ("*") + kExt))
        out.add ({ f.getFileNameWithoutExtension(), scope, f, f.getLastModificationTime() });
    std::sort (out.begin(), out.end(), [] (const PresetInfo& a, const PresetInfo& b) { return a.name.compareNatural (b.name) < 0; });
    return out;
}

juce::Result PresetStore::validate (const juce::var& v)
{
    if (! v.isObject()) return juce::Result::fail ("not a JSON object");
    if (v["format"].toString() != kPresetFormat) return juce::Result::fail ("not a GLOBAL CONTROL preset");
    const int ver = (int) v["version"];
    if (ver < 1) return juce::Result::fail ("missing version");
    if (ver > kPresetVersion) return juce::Result::fail ("preset made by a newer GLOBAL CONTROL (version " + juce::String (ver) + ")");
    if (! v["macros"].isArray()) return juce::Result::fail ("no macros");
    return juce::Result::ok();
}

static juce::Result writeJson (const juce::File& f, const juce::var& data)
{
    if (! f.getParentDirectory().createDirectory()) return juce::Result::fail ("cannot create " + f.getParentDirectory().getFullPathName());
    juce::TemporaryFile tmp (f);
    if (! tmp.getFile().replaceWithText (juce::JSON::toString (data, false))) return juce::Result::fail ("cannot write " + f.getFullPathName());
    if (! tmp.overwriteTargetFileWithTemporary()) return juce::Result::fail ("cannot replace " + f.getFullPathName());
    return juce::Result::ok();
}

juce::Result PresetStore::save (PresetScope scope, const juce::String& projectId, const juce::String& name, const juce::var& data, bool overwrite)
{
    const auto f = fileFor (scope, projectId, name);
    if (f.existsAsFile() && ! overwrite) return juce::Result::fail ("a preset named \"" + sanitize (name) + "\" already exists");
    if (auto* o = data.getDynamicObject()) {
        o->setProperty ("name", sanitize (name));
        o->setProperty ("scope", scope == PresetScope::Global ? "global" : "project");
    }
    return writeJson (f, data);
}

juce::Result PresetStore::load (const PresetInfo& p, juce::var& out) const
{
    if (! p.file.existsAsFile()) return juce::Result::fail ("preset file not found");
    out = juce::JSON::parse (p.file.loadFileAsString());
    return validate (out);
}

juce::Result PresetStore::rename (const PresetInfo& p, const juce::String& newName)
{
    const auto target = p.file.getParentDirectory().getChildFile (sanitize (newName) + kExt);
    if (target == p.file) return juce::Result::ok();
    if (target.exists()) return juce::Result::fail ("a preset named \"" + sanitize (newName) + "\" already exists");
    juce::var data;
    if (auto r = load (p, data); r.failed()) return r;
    if (auto* o = data.getDynamicObject()) o->setProperty ("name", sanitize (newName));
    if (auto r = writeJson (target, data); r.failed()) return r;
    return p.file.deleteFile() ? juce::Result::ok() : juce::Result::fail ("cannot remove the old file");
}

juce::Result PresetStore::remove (const PresetInfo& p)
{
    return p.file.moveToTrash() || p.file.deleteFile() ? juce::Result::ok() : juce::Result::fail ("cannot delete " + p.file.getFullPathName());
}

juce::Result PresetStore::duplicate (const PresetInfo& p, juce::String& newName)
{
    juce::var data;
    if (auto r = load (p, data); r.failed()) return r;
    int n = 2;
    do newName = p.name + " " + juce::String (n++);
    while (p.file.getParentDirectory().getChildFile (sanitize (newName) + kExt).exists());
    if (auto* o = data.getDynamicObject()) o->setProperty ("name", newName);
    return writeJson (p.file.getParentDirectory().getChildFile (sanitize (newName) + kExt), data);
}

void PresetStore::saveSession (const juce::String& projectId, const juce::var& data)
{
    if (projectId.isEmpty()) return;
    writeJson (root.getChildFile ("Sessions").getChildFile (sanitize (projectId) + ".json"), data);
}

juce::var PresetStore::loadSession (const juce::String& projectId) const
{
    const auto f = root.getChildFile ("Sessions").getChildFile (sanitize (projectId) + ".json");
    if (projectId.isEmpty() || ! f.existsAsFile()) return {};
    auto v = juce::JSON::parse (f.loadFileAsString());
    return validate (v).wasOk() ? v : juce::var();
}

} // namespace gc
