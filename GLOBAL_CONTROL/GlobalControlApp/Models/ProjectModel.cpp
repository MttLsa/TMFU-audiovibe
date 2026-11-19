#include "ProjectModel.h"

#include <map>

namespace gc {

static juce::String s (const juce::var& o, const char* k) { const auto& v = o[k]; return v.isVoid() ? juce::String() : v.toString(); }
static int i (const juce::var& o, const char* k) { return (int) o[k]; }
static bool b (const juce::var& o, const char* k) { return (int) o[k] != 0; }
static double d (const juce::var& o, const char* k) { return (double) o[k]; }

juce::String DeviceInfo::location() const
{
    juce::String l = trackName;
    const juce::String dot (juce::CharPointer_UTF8 (" \xc2\xb7 "));
    if (rackPath.isNotEmpty()) l << dot << rackPath.trimCharactersAtEnd (" >");
    l << dot << "slot " << (position + 1);
    return l;
}

DeviceFingerprint DeviceInfo::fingerprint (const juce::String& projectId) const
{
    DeviceFingerprint f;
    f.runtimeId = id.toStdString();
    f.projectId = projectId.toStdString();
    f.className = className.toStdString();
    f.classDisplayName = classDisplayName.toStdString();
    f.paramNamesHash = paramNamesHash.toStdString();
    f.deviceName = name.toStdString();
    f.trackName = trackName.toStdString();
    f.locationKey = key.toStdString();
    f.trackIndex = trackIndex;
    return f;
}

juce::String ParamInfo::statusText() const
{
    if (! enabled) return "locked (macro/modulation)";
    if (state == 2) return "disabled";
    if (state == 1) return "inactive";
    if (automationState == 1) return "automated";
    if (automationState == 2) return "automation overridden";
    return quantized ? "stepped" : "continuous";
}

ProjectSnapshot ProjectSnapshot::fromJson (const juce::var& doc)
{
    ProjectSnapshot p;
    p.projectId = s (doc, "projectId");
    std::map<juce::String, int> trackIndexById;
    if (auto* ts = doc["tracks"].getArray()) {
        for (const auto& t : *ts) {
            TrackInfo ti;
            ti.id = s (t, "id"); ti.key = s (t, "key"); ti.kind = s (t, "kind"); ti.name = s (t, "name");
            ti.index = i (t, "index"); ti.colour = i (t, "color");
            ti.isMidi = b (t, "isMidi"); ti.isGroup = b (t, "isGroup"); ti.isBridgeTrack = b (t, "isBridgeTrack");
            trackIndexById[ti.id] = ti.kind == "track" ? ti.index : ti.kind == "return" ? 1000 + ti.index : 2000;
            p.tracks.add (ti);
        }
    }
    if (auto* ds = doc["devices"].getArray()) {
        for (const auto& o : *ds) {
            DeviceInfo di;
            di.id = s (o, "id"); di.key = s (o, "key"); di.trackId = s (o, "trackId"); di.trackName = s (o, "trackName");
            di.trackKey = s (o, "trackKey"); di.trackKind = s (o, "trackKind"); di.rackPath = s (o, "rackPath");
            di.name = s (o, "name"); di.className = s (o, "className"); di.classDisplayName = s (o, "classDisplayName");
            di.format = s (o, "format"); di.paramNamesHash = s (o, "paramNamesHash");
            di.position = i (o, "position"); di.depth = i (o, "depth"); di.type = i (o, "type"); di.paramCount = i (o, "paramCount");
            di.isPlugin = b (o, "isPlugin"); di.isActive = o["isActive"].isVoid() || b (o, "isActive");
            di.canHaveChains = b (o, "canHaveChains"); di.isBridge = b (o, "isBridge");
            const auto it = trackIndexById.find (di.trackId);
            di.trackIndex = it != trackIndexById.end() ? it->second : -1;
            p.devices.add (di);
        }
    }
    p.scannedAt = juce::Time::getCurrentTime();
    p.valid = true;
    return p;
}

const DeviceInfo* ProjectSnapshot::findDevice (const juce::String& id) const
{
    for (const auto& dv : devices) if (dv.id == id) return &dv;
    return nullptr;
}

std::vector<DeviceFingerprint> ProjectSnapshot::fingerprints() const
{
    std::vector<DeviceFingerprint> v;
    for (const auto& dv : devices) v.push_back (dv.fingerprint (projectId));
    InstanceReconciler::assignOrdinals (v);
    return v;
}

int ProjectSnapshot::countInstances() const
{
    int n = 0;
    for (const auto& dv : devices) if (! dv.isBridge && dv.isPlugin) ++n;
    return n;
}

juce::Array<ParamInfo> parseParameters (const juce::var& doc)
{
    juce::Array<ParamInfo> out;
    if (auto* ps = doc["parameters"].getArray()) {
        for (const auto& o : *ps) {
            ParamInfo p;
            p.id = o["id"].toString().getLargeIntValue();
            p.index = i (o, "index"); p.name = s (o, "name"); p.originalName = s (o, "originalName");
            p.display = s (o, "display"); p.value = d (o, "value"); p.min = d (o, "min"); p.max = d (o, "max");
            p.quantized = b (o, "isQuantized"); p.enabled = o["isEnabled"].isVoid() || b (o, "isEnabled");
            p.state = i (o, "state"); p.automationState = i (o, "automationState");
            p.hasDefault = ! o["defaultValue"].isVoid() && ! o["defaultValue"].isUndefined();
            p.defaultValue = p.hasDefault ? d (o, "defaultValue") : p.min;
            if (auto* it = o["valueItems"].getArray()) for (const auto& x : *it) p.items.add (x.toString());
            if (p.originalName.isEmpty()) p.originalName = p.name;
            out.add (p);
        }
    }
    return out;
}

juce::var fingerprintToVar (const DeviceFingerprint& f)
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("className", juce::String (f.className));
    o->setProperty ("classDisplayName", juce::String (f.classDisplayName));
    o->setProperty ("paramNamesHash", juce::String (f.paramNamesHash));
    o->setProperty ("deviceName", juce::String (f.deviceName));
    o->setProperty ("trackName", juce::String (f.trackName));
    o->setProperty ("locationKey", juce::String (f.locationKey));
    o->setProperty ("trackIndex", f.trackIndex);
    o->setProperty ("ordinalOnTrack", f.ordinalOnTrack);
    o->setProperty ("projectId", juce::String (f.projectId));
    return juce::var (o);
}

DeviceFingerprint fingerprintFromVar (const juce::var& v)
{
    DeviceFingerprint f;
    f.className = s (v, "className").toStdString();
    f.classDisplayName = s (v, "classDisplayName").toStdString();
    f.paramNamesHash = s (v, "paramNamesHash").toStdString();
    f.deviceName = s (v, "deviceName").toStdString();
    f.trackName = s (v, "trackName").toStdString();
    f.locationKey = s (v, "locationKey").toStdString();
    f.trackIndex = v["trackIndex"].isVoid() ? -1 : i (v, "trackIndex");
    f.ordinalOnTrack = i (v, "ordinalOnTrack");
    f.projectId = s (v, "projectId").toStdString();
    return f;
}

} // namespace gc
