#include "LoopLibrary.h"
#include "BinaryData.h"
#include <random>

using namespace juce;

namespace
{
    constexpr int kDbVersion = 3;
    const char* const audioWildcard = "*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg;*.m4a;*.caf";

    // feature weights for the similarity space (drums vs tonal material)
    float featureWeight (int f, bool tonal)
    {
        if (f < 13)  return 1.0f;                       // MFCC means: timbre
        if (f < 26)  return 0.45f;                      // MFCC std: timbre movement
        if (f < 34)  return 0.8f;                       // spectral shape + dynamics
        if (f < 37)  return 0.9f;                       // low / mid / high balance
        if (f == 37) return tonal ? 0.5f : 1.0f;        // onset density
        if (f < 54)  return tonal ? 0.15f : 0.55f;      // rhythm pattern
        if (f < 66)  return tonal ? 0.5f : 0.0f;        // key-relative chroma (harmonic colour)
        return tonal ? 0.4f : 0.0f;                     // voiced ratio
    }

    var toVar (const std::vector<float>& v)
    {
        Array<var> a; a.ensureStorageAllocated ((int) v.size());
        for (auto x : v) a.add (std::round (x * 10000.0f) / 10000.0f);
        return a;
    }
    std::vector<float> fromVar (const var& v)
    {
        std::vector<float> r;
        if (auto* a = v.getArray()) for (auto& x : *a) r.push_back ((float) (double) x);
        return r;
    }
}

int LoopDatabase::indexOf (const String& path) const
{
    for (size_t i = 0; i < entries.size(); ++i) if (entries[i].path == path) return (int) i;
    return -1;
}

//==============================================================================
LoopLibrary::LoopLibrary() : Thread ("Groovulation scan")
{
    fm.registerBasicFormats();
    PropertiesFile::Options o;
    o.applicationName = "settings";
    o.filenameSuffix = ".xml";
    o.storageFormat = PropertiesFile::storeAsXML;
    rootFolder().createDirectory();
    settings = std::make_unique<PropertiesFile> (rootFolder().getChildFile ("settings.xml"), o);
    for (auto f : { factoryFolder(), printFolder(), snapshotFolder(), databaseFolder() }) f.createDirectory();
    installFactoryIfMissing();
    database = loadDatabaseFile();
    if (database == nullptr) database = std::make_shared<LoopDatabase>();
    startScan (false);   // quick: only new / changed files are analysed
}

LoopLibrary::~LoopLibrary()
{
    stopThread (8000);
}

File LoopLibrary::rootFolder()
{
   #if JUCE_LINUX
    auto base = File::getSpecialLocation (File::userDocumentsDirectory);
   #else
    auto base = File::getSpecialLocation (File::commonDocumentsDirectory);   // macOS: /Users/Shared
   #endif
    return base.getChildFile ("Groovulation");
}

void LoopLibrary::installFactoryIfMissing()
{
    MemoryInputStream in (BinaryData::factory_loops_zip, BinaryData::factory_loops_zipSize, false);
    ZipFile zip (in);
    var embedded;
    if (auto* e = zip.getEntry ("Loops/manifest.json"))
        if (std::unique_ptr<InputStream> s (zip.createStreamForEntry (*e)); s != nullptr)
            embedded = JSON::parse (s->readEntireStreamAsString());
    const auto installedFile = factoryFolder().getChildFile ("manifest.json");
    const var installed = installedFile.existsAsFile() ? JSON::parse (installedFile.loadFileAsString()) : var();
    const int vEmb = (int) embedded.getProperty ("version", 1), vInst = installed.isObject() ? (int) installed.getProperty ("version", 0) : 0;
    bool missing = false;
    for (int m = 0; m < lb::numModules; ++m)
        if (factoryFolder().getChildFile (lb::folderName (m)).findChildFiles (File::findFiles, false, audioWildcard).isEmpty()) missing = true;
    if (! missing && vInst >= vEmb) return;
    // upgrade: remove the files of the previously installed factory set (user files are never touched)
    if (installed.isObject() && vInst < vEmb)
        if (auto* folders = installed.getProperty ("folders", var()).getDynamicObject())
            for (auto& fo : folders->getProperties())
                if (auto* files = fo.value.getArray())
                    for (auto& f : *files)
                    {
                        const String name = f.getProperty ("file", "").toString();
                        if (name.isNotEmpty() && ! name.containsAnyOf ("/\\"))
                            factoryFolder().getChildFile (fo.name.toString()).getChildFile (name).deleteFile();
                    }
    const auto r = zip.uncompressTo (rootFolder(), true);
    if (r.failed()) DBG ("Groovulation: factory loop install failed: " << r.getErrorMessage());
}

StringArray LoopLibrary::userFolders() const
{
    const ScopedLock sl (settingsLock);
    StringArray a;
    a.addLines (settings->getValue ("userFolders"));
    a.removeEmptyStrings();
    return a;
}

void LoopLibrary::addUserFolder (const File& f)
{
    {
        const ScopedLock sl (settingsLock);
        auto a = userFolders();
        a.addIfNotAlreadyThere (f.getFullPathName());
        settings->setValue ("userFolders", a.joinIntoString ("\n"));
        settings->saveIfNeeded();
    }
    startScan (false);
}

void LoopLibrary::removeUserFolder (const String& path)
{
    {
        const ScopedLock sl (settingsLock);
        auto a = userFolders();
        a.removeString (path);
        settings->setValue ("userFolders", a.joinIntoString ("\n"));
        settings->saveIfNeeded();
    }
    startScan (false);
}

void LoopLibrary::startScan (bool full)
{
    if (full) rescanAll = true;
    if (isThreadRunning())
    {
        // a scan is running: ask it to restart with the new folder set
        signalThreadShouldExit();
        stopThread (10000);
    }
    scanning = true;
    progress = 0.0f;
    startThread (Thread::Priority::low);
}

//==============================================================================
void LoopLibrary::run()
{
    const bool full = rescanAll.exchange (false);
    auto old = db();
    std::map<String, const LoopEntry*> cache;
    if (! full && old != nullptr) for (auto& e : old->entries) cache[e.path] = &e;

    // collect files: factory folder + custom folders (recursive)
    struct Item { File file; String keywords; };
    std::vector<Item> items;
    auto collect = [&items] (const File& root)
    {
        if (! root.isDirectory()) return;
        for (const auto& entry : RangedDirectoryIterator (root, true, audioWildcard, File::findFiles))
        {
            const auto f = entry.getFile();
            if (f.getFileName().startsWithChar ('.')) continue;
            items.push_back ({ f, f.getRelativePathFrom (root.getParentDirectory()) });
        }
    };
    collect (factoryFolder());
    for (auto& p : userFolders()) collect (File (p));

    auto d = std::make_shared<LoopDatabase>();
    d->entries.reserve (items.size());
    int done = 0, analysed = 0;
    for (auto& it : items)
    {
        if (threadShouldExit()) { scanning = false; return; }
        LoopEntry e;
        e.path = it.file.getFullPathName();
        e.name = it.file.getFileNameWithoutExtension();
        e.size = it.file.getSize();
        e.mtime = it.file.getLastModificationTime().toMilliseconds();
        auto c = cache.find (e.path);
        if (c != cache.end() && c->second->size == e.size && c->second->mtime == e.mtime)
            e.info = c->second->info;
        else
        {
            {
                const ScopedLock sl (statusLock);
                status = "Analysing " + e.name;
            }
            e.info = analysis::analyse (fm, it.file, it.keywords);
            ++analysed;
        }
        if (e.info.ok && e.info.type >= 0) d->entries.push_back (std::move (e));
        progress = (float) ++done / (float) jmax<size_t> (1, items.size());
    }
    // files without a type keyword: classified by their nearest named neighbours
    {
        std::vector<LoopEntry> labelled;
        for (auto& e : d->entries) if (e.info.typeFromName) labelled.push_back (e);
        if (labelled.size() >= 9)
            for (auto& e : d->entries) if (! e.info.typeFromName) e.info.type = classifyByNeighbours (labelled, e.info);
    }
    finalise (*d);
    std::atomic_store (&database, DbPtr (d));
    if (analysed > 0 || old == nullptr || old->entries.size() != d->entries.size()) saveDatabase (*d);
    {
        const ScopedLock sl (statusLock);
        status = String ((int) d->entries.size()) + " loops in the database";
    }
    progress = 1.0f;
    scanning = false;
    sendChangeMessage();
}

void LoopLibrary::finalise (LoopDatabase& d) const
{
    for (auto& v : d.byType) v.clear();
    std::sort (d.entries.begin(), d.entries.end(), [] (const LoopEntry& a, const LoopEntry& b) { return a.name.compareNatural (b.name) < 0; });
    for (int i = 0; i < (int) d.entries.size(); ++i) d.byType[(size_t) d.entries[(size_t) i].info.type].push_back (i);

    // per type: z-score normalisation + weights -> similarity vectors, then k-means "families"
    for (int t = 0; t < lb::numModules; ++t)
    {
        auto& idx = d.byType[(size_t) t];
        if (idx.empty()) continue;
        const bool tonal = lb::isTonal (t);
        const int F = analysis::numFeatures;
        std::vector<double> mean ((size_t) F, 0.0), sd ((size_t) F, 0.0);
        for (int i : idx) for (int f = 0; f < F; ++f) mean[(size_t) f] += d.entries[(size_t) i].info.features[(size_t) f];
        for (auto& m : mean) m /= (double) idx.size();
        for (int i : idx) for (int f = 0; f < F; ++f) { const double x = d.entries[(size_t) i].info.features[(size_t) f] - mean[(size_t) f]; sd[(size_t) f] += x * x; }
        for (auto& s : sd) s = std::sqrt (s / (double) idx.size()) + 1e-6;
        for (int i : idx)
        {
            auto& e = d.entries[(size_t) i];
            e.vec.resize ((size_t) F);
            for (int f = 0; f < F; ++f)
            {
                double z = (e.info.features[(size_t) f] - mean[(size_t) f]) / sd[(size_t) f];
                if (idx.size() < 3) z = e.info.features[(size_t) f];   // too few loops for statistics
                e.vec[(size_t) f] = (float) (jlimit (-4.0, 4.0, z) * std::sqrt (featureWeight (f, tonal)));
            }
        }
        // k-means++ (deterministic)
        const int n = (int) idx.size();
        const int k = jlimit (1, 24, (int) std::lround (std::sqrt (n / 2.0)));
        std::mt19937 rng (1234u + (unsigned) t);
        auto dist2 = [&] (const std::vector<float>& a, const std::vector<float>& b)
        { double s = 0; for (size_t f = 0; f < a.size(); ++f) { const double q = a[f] - b[f]; s += q * q; } return s; };
        std::vector<std::vector<float>> centres;
        centres.push_back (d.entries[(size_t) idx[0]].vec);
        while ((int) centres.size() < k)
        {
            std::vector<double> dd ((size_t) n); double tot = 0;
            for (int j = 0; j < n; ++j)
            {
                double best = 1e30; for (auto& c : centres) best = std::min (best, dist2 (d.entries[(size_t) idx[(size_t) j]].vec, c));
                dd[(size_t) j] = best; tot += best;
            }
            double r = std::uniform_real_distribution<double> (0.0, tot) (rng);
            int pick = 0; for (; pick < n - 1; ++pick) { r -= dd[(size_t) pick]; if (r <= 0) break; }
            centres.push_back (d.entries[(size_t) idx[(size_t) pick]].vec);
        }
        std::vector<int> assign ((size_t) n, 0);
        for (int iter = 0; iter < 25; ++iter)
        {
            bool changed = false;
            for (int j = 0; j < n; ++j)
            {
                int best = 0; double bd = 1e30;
                for (int c = 0; c < k; ++c) { const double q = dist2 (d.entries[(size_t) idx[(size_t) j]].vec, centres[(size_t) c]); if (q < bd) { bd = q; best = c; } }
                if (assign[(size_t) j] != best) { assign[(size_t) j] = best; changed = true; }
            }
            for (int c = 0; c < k; ++c)
            {
                std::vector<float> acc ((size_t) F, 0.0f); int cnt = 0;
                for (int j = 0; j < n; ++j) if (assign[(size_t) j] == c) { for (int f = 0; f < F; ++f) acc[(size_t) f] += d.entries[(size_t) idx[(size_t) j]].vec[(size_t) f]; ++cnt; }
                if (cnt > 0) { for (auto& a : acc) a /= (float) cnt; centres[(size_t) c] = acc; }
            }
            if (! changed && iter > 0) break;
        }
        for (int j = 0; j < n; ++j) d.entries[(size_t) idx[(size_t) j]].family = assign[(size_t) j] + 1;
    }
}

std::vector<int> LoopLibrary::candidates (const LoopDatabase& d, int type, double hostBpm) const
{
    std::vector<int> r;
    const double maxSec = 32.0 * 4.0 * 60.0 / jmax (20.0, hostBpm) + 0.05;   // 32 bars at the project tempo
    for (int i : d.byType[(size_t) jlimit (0, lb::numModules - 1, type)])
    {
        const auto& e = d.entries[(size_t) i];
        if (e.info.duration > maxSec || e.info.beats > 128) continue;   // longer than 32 bars
        if (e.info.duration < 0.9 || e.info.beats < 4) continue;        // shorter than one bar (one-shots)
        r.push_back (i);
    }
    return r;
}

int LoopLibrary::nearest (const LoopDatabase& d, int index, const std::set<int>& exclude, const std::vector<int>& pool)
{
    if (index < 0 || index >= (int) d.entries.size()) return -1;
    const auto& a = d.entries[(size_t) index];
    int best = -1; double bd = 1e30;
    for (int i : pool)
    {
        if (i == index || exclude.count (i) > 0) continue;
        const auto& b = d.entries[(size_t) i];
        if (a.vec.size() != b.vec.size()) continue;
        double s = 0; for (size_t f = 0; f < a.vec.size(); ++f) { const double q = a.vec[f] - b.vec[f]; s += q * q; }
        if (b.family != a.family) s *= 1.15;   // stay inside the family when possible
        if (s < bd) { bd = s; best = i; }
    }
    return best;
}

int LoopLibrary::classifyByNeighbours (const std::vector<LoopEntry>& lab, const analysis::Result& r, int k)
{
    if (lab.empty() || r.features.size() != (size_t) analysis::numFeatures) return r.type;
    const int F = analysis::numFeatures;
    // descriptors that tell material types apart: timbre, spectral balance, dynamics, density, pitchedness
    auto use = [] (int f) { return f < 13 || (f >= 26 && f <= 37) || f == 66; };
    std::vector<double> mean ((size_t) F, 0.0), sd ((size_t) F, 0.0);
    for (auto& e : lab) for (int f = 0; f < F; ++f) mean[(size_t) f] += e.info.features[(size_t) f];
    for (auto& m : mean) m /= (double) lab.size();
    for (auto& e : lab) for (int f = 0; f < F; ++f) { const double q = e.info.features[(size_t) f] - mean[(size_t) f]; sd[(size_t) f] += q * q; }
    for (auto& v : sd) v = std::sqrt (v / (double) lab.size()) + 1e-6;
    std::vector<std::pair<double, int>> dist;
    for (auto& e : lab)
    {
        if (e.info.features == r.features) continue;   // (leave-one-out when testing)
        double s = 0;
        for (int f = 0; f < F; ++f)
            if (use (f)) { const double q = (e.info.features[(size_t) f] - r.features[(size_t) f]) / sd[(size_t) f]; s += q * q; }
        dist.push_back ({ s, e.info.type });
    }
    std::sort (dist.begin(), dist.end());
    std::array<double, lb::numModules> votes {};
    for (int i = 0; i < jmin (k, (int) dist.size()); ++i) votes[(size_t) dist[(size_t) i].second] += 1.0 / (1e-3 + std::sqrt (dist[(size_t) i].first));
    int best = r.type < 0 ? 0 : r.type; double bv = 0;
    for (int t = 0; t < lb::numModules; ++t) if (votes[(size_t) t] > bv) { bv = votes[(size_t) t]; best = t; }
    return best;
}

//==============================================================================
LoopPtr LoopLibrary::load (const LoopEntry& e, double targetRate)
{
    AudioFormatManager fm; fm.registerBasicFormats();
    std::unique_ptr<AudioFormatReader> reader (fm.createReaderFor (File (e.path)));
    if (reader == nullptr) return nullptr;
    const int len = (int) std::min<int64> (reader->lengthInSamples, (int64) (reader->sampleRate * 600.0));
    AudioBuffer<float> src (2, len);
    reader->read (&src, 0, len, 0, true, reader->numChannels > 1);
    if (reader->numChannels == 1) src.copyFrom (1, 0, src, 0, 0, len);

    auto d = std::make_shared<LoopData>();
    d->path = e.path; d->name = e.name; d->type = e.info.type;
    d->bpm = e.info.bpm; d->beats = e.info.beats; d->info = e.info;
    d->sampleRate = targetRate;
    const double ratio = reader->sampleRate / targetRate;
    if (std::abs (ratio - 1.0) < 1e-6)
        d->audio = std::move (src);
    else
    {
        const int outLen = (int) std::floor (len / ratio);
        d->audio.setSize (2, outLen);
        for (int c = 0; c < 2; ++c)
        {
            WindowedSincInterpolator interp;
            interp.process (ratio, src.getReadPointer (c), d->audio.getWritePointer (c), outLen, len, 0);
        }
    }
    // onsets at the playback rate (more precise than the analysis ones)
    {
        std::vector<float> mono ((size_t) d->audio.getNumSamples());
        for (int i = 0; i < d->audio.getNumSamples(); ++i) mono[(size_t) i] = 0.5f * (d->audio.getSample (0, i) + d->audio.getSample (1, i));
        d->onsets = analysis::detectOnsets (mono.data(), (int) mono.size(), targetRate);
        const int bins = 1024;
        d->overview.assign (bins, 0.0f);
        float mx = 1e-6f;
        for (int b = 0; b < bins; ++b)
        {
            const int s0 = (int) ((int64) b * (int64) mono.size() / bins), s1 = (int) ((int64) (b + 1) * (int64) mono.size() / bins);
            float p = 0; for (int i = s0; i < s1; ++i) p = jmax (p, std::abs (mono[(size_t) i]));
            d->overview[(size_t) b] = p; mx = jmax (mx, p);
        }
        for (auto& v : d->overview) v /= mx;
    }
    return d;
}

//==============================================================================
void LoopLibrary::saveDatabase (const LoopDatabase& d) const
{
    databaseFolder().createDirectory();
    Array<var> arr;
    for (auto& e : d.entries)
    {
        auto* o = new DynamicObject();
        const auto& r = e.info;
        o->setProperty ("path", e.path); o->setProperty ("size", (juce::int64) e.size); o->setProperty ("mtime", (juce::int64) e.mtime);
        o->setProperty ("sr", r.sampleRate); o->setProperty ("n", (juce::int64) r.numSamples); o->setProperty ("ch", r.channels); o->setProperty ("dur", r.duration);
        o->setProperty ("type", r.type); o->setProperty ("typeName", r.typeFromName);
        o->setProperty ("bpm", r.bpm); o->setProperty ("beats", r.beats); o->setProperty ("bpmName", r.bpmFromName);
        o->setProperty ("key", r.keyRoot); o->setProperty ("minor", r.minor); o->setProperty ("keyConf", r.keyConf); o->setProperty ("tune", r.tuneCents);
        o->setProperty ("nameKey", r.nameRoot); o->setProperty ("nameMinor", r.nameMinor);
        o->setProperty ("f0", r.f0); o->setProperty ("voiced", r.voiced);
        o->setProperty ("chroma", toVar (std::vector<float> (r.chroma.begin(), r.chroma.end())));
        o->setProperty ("features", toVar (r.features));
        o->setProperty ("onsets", toVar (r.onsets));
        arr.add (var (o));
    }
    auto* root = new DynamicObject();
    root->setProperty ("version", kDbVersion);
    root->setProperty ("loops", arr);
    databaseFolder().getChildFile ("loopdb.json").replaceWithText (JSON::toString (var (root), true));

    // human readable registry of similar loops: type -> family -> loops
    auto* reg = new DynamicObject();
    for (int t = 0; t < lb::numModules; ++t)
    {
        std::map<int, Array<var>> fam;
        for (int i : d.byType[(size_t) t])
        {
            const auto& e = d.entries[(size_t) i];
            String line = e.name + "  |  " + String (e.info.bpm, 1) + " BPM, " + String (e.info.beats / 4.0, 2) + " bars";
            if (lb::isTonal (t)) line << ", " << analysis::keyName (e.info.keyRoot, e.info.minor) << ", f0 " << analysis::noteName (e.info.f0);
            fam[e.family].add (line);
        }
        auto* fo = new DynamicObject();
        for (auto& f : fam) fo->setProperty ("family " + String (f.first), f.second);
        reg->setProperty (lb::moduleName (t), var (fo));
    }
    databaseFolder().getChildFile ("similar_loops.json").replaceWithText (JSON::toString (var (reg), false));
}

DbPtr LoopLibrary::loadDatabaseFile() const
{
    const auto f = databaseFolder().getChildFile ("loopdb.json");
    if (! f.existsAsFile()) return nullptr;
    const var root = JSON::parse (f);
    if ((int) root.getProperty ("version", 0) != kDbVersion) return nullptr;
    auto d = std::make_shared<LoopDatabase>();
    if (auto* arr = root.getProperty ("loops", var()).getArray())
        for (auto& o : *arr)
        {
            LoopEntry e;
            e.path = o.getProperty ("path", "").toString();
            e.name = File (e.path).getFileNameWithoutExtension();
            e.size = (int64) o.getProperty ("size", 0); e.mtime = (int64) o.getProperty ("mtime", 0);
            auto& r = e.info;
            r.ok = true;
            r.sampleRate = o.getProperty ("sr", 44100.0); r.numSamples = (int64) o.getProperty ("n", 0); r.channels = o.getProperty ("ch", 1); r.duration = o.getProperty ("dur", 0.0);
            r.type = o.getProperty ("type", 0); r.typeFromName = o.getProperty ("typeName", false);
            r.bpm = o.getProperty ("bpm", 120.0); r.beats = o.getProperty ("beats", 4); r.bpmFromName = o.getProperty ("bpmName", false);
            r.keyRoot = o.getProperty ("key", -1); r.minor = o.getProperty ("minor", false); r.keyConf = (float) (double) o.getProperty ("keyConf", 0.0); r.tuneCents = (float) (double) o.getProperty ("tune", 0.0);
            r.nameRoot = o.getProperty ("nameKey", -1); r.nameMinor = o.getProperty ("nameMinor", false);
            r.f0 = (float) (double) o.getProperty ("f0", 0.0); r.voiced = (float) (double) o.getProperty ("voiced", 0.0);
            const auto ch = fromVar (o.getProperty ("chroma", var()));
            for (size_t i = 0; i < 12 && i < ch.size(); ++i) r.chroma[i] = ch[i];
            r.features = fromVar (o.getProperty ("features", var()));
            r.onsets = fromVar (o.getProperty ("onsets", var()));
            if ((int) r.features.size() != analysis::numFeatures || r.type < 0 || r.type >= lb::numModules) continue;
            d->entries.push_back (std::move (e));
        }
    finalise (*d);
    return d;
}
