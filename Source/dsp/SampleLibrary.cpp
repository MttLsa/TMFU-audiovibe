#include "SampleLibrary.h"
#include <BinaryData.h>

using namespace juce;

namespace
{
    const char* moduleNames[] { "Kick", "Snare", "Hat", "Tie", "Pod" };
    const char* sourceKeys[]  { "twinbox", "legacy", "user" };

    Array<File> audioFilesIn (const File& folder)
    {
        Array<File> files;
        if (! folder.isDirectory()) return files;
        for (auto& f : folder.findChildFiles (File::findFiles, false, "*.wav;*.aif;*.aiff;*.flac;*.ogg"))
            if (! f.getFileName().startsWithChar ('.')) files.add (f);
        struct Cmp { static int compareElements (const File& a, const File& b) { return a.getFileName().compareNatural (b.getFileName()); } } cmp;
        files.sort (cmp);
        return files;
    }
}

//==============================================================================
SampleLibrary::SampleLibrary()
{
    sources.fill (twinbox);
    for (auto& s : sets) s = std::make_shared<SampleSet>();
}

SampleLibrary::~SampleLibrary()
{
    pool.removeAllJobs (true, 5000);
}

File SampleLibrary::rootFolder()
{
   #if JUCE_LINUX
    auto base = File::getSpecialLocation (File::userDocumentsDirectory);
   #else
    auto base = File::getSpecialLocation (File::commonDocumentsDirectory);   // macOS: /Users/Shared
   #endif
    return base.getChildFile ("Twinbox").getChildFile ("Samples");
}

String SampleLibrary::moduleName (int m) { return moduleNames[jlimit (0, numModules - 1, m)]; }

File SampleLibrary::folderFor (int module, Source s)
{
    return rootFolder().getChildFile ((s == legacy ? "Legacy " : "Twinbox ") + moduleName (module));
}

StringArray SampleLibrary::namesIn (const File& folder)
{
    StringArray n;
    for (auto& f : audioFilesIn (folder)) n.add (f.getFileNameWithoutExtension());
    return n;
}

bool SampleLibrary::hasLegacy()
{
    return ! audioFilesIn (folderFor (kick, legacy)).isEmpty();
}

void SampleLibrary::installFactoryIfMissing()
{
    const auto root = rootFolder();
    bool missing = false;
    for (int m = 0; m < numModules; ++m)
        if (audioFilesIn (folderFor (m, twinbox)).isEmpty()) missing = true;
    if (! missing) return;
    root.createDirectory();
    MemoryInputStream in (BinaryData::factory_samples_zip, BinaryData::factory_samples_zipSize, false);
    ZipFile zip (in);
    const auto r = zip.uncompressTo (root, false);   // never overwrite user changes
    if (r.failed()) DBG ("Twinbox: factory sample install failed: " << r.getErrorMessage());
}

File SampleLibrary::currentFolder (int m) const
{
    const auto s = sources[(size_t) m];
    if (s == user && userFolders[(size_t) m].isDirectory()) return userFolders[(size_t) m];
    return folderFor (m, s == user ? twinbox : s);
}

void SampleLibrary::setSource (int m, Source s)
{
    sources[(size_t) m] = s;
    reload (m);
}

void SampleLibrary::setUserFolder (int m, const File& f)
{
    userFolders[(size_t) m] = f;
    sources[(size_t) m] = user;
    reload (m);
}

void SampleLibrary::reloadAll()
{
    for (int m = 0; m < numModules; ++m) reload (m);
}

SamplePtr SampleLibrary::loadFile (AudioFormatManager& fm, const File& f)
{
    std::unique_ptr<AudioFormatReader> r (fm.createReaderFor (f));
    if (r == nullptr || r->lengthInSamples <= 1) return nullptr;
    const int len = (int) jmin<int64> (r->lengthInSamples, (int64) (r->sampleRate * 30.0));
    AudioBuffer<float> buf ((int) jmin (2u, r->numChannels), len);
    r->read (&buf, 0, len, 0, true, buf.getNumChannels() > 1);
    auto s = std::make_shared<Sample>();
    s->name = f.getFileNameWithoutExtension();
    s->sourceRate = r->sampleRate;
    s->L.assign (buf.getReadPointer (0), buf.getReadPointer (0) + len);
    if (buf.getNumChannels() > 1) s->R.assign (buf.getReadPointer (1), buf.getReadPointer (1) + len);
    return s;
}

void SampleLibrary::reload (int m)
{
    int gen;
    {
        const ScopedLock sl (lock);
        gen = ++generation[(size_t) m];
    }
    const File folder = currentFolder (m);
    ++pending;
    pool.addJob ([this, m, gen, folder]
    {
        AudioFormatManager fm;
        fm.registerBasicFormats();
        auto set = std::make_shared<SampleSet>();
        set->folder = folder;
        for (auto& f : audioFilesIn (folder))
        {
            if (auto s = loadFile (fm, f))
            {
                set->samples.push_back (s);
                set->names.add (s->name);
                SamplePtr open;
                if (m == hat)
                {
                    const auto partner = folder.getChildFile ("open").getChildFile (f.getFileName());
                    if (partner.existsAsFile()) open = loadFile (fm, partner);
                }
                set->openPartner.push_back (open);
            }
        }
        bool current;
        {
            const ScopedLock sl (lock);
            current = generation[(size_t) m] == gen;
        }
        if (current)
        {
            std::atomic_store (&sets[(size_t) m], SampleSetPtr (set));
            sendChangeMessage();
        }
        --pending;
    });
}

int SampleLibrary::indexOf (int m, const String& name) const
{
    auto s = set (m);
    const int i = s->names.indexOf (name, true);
    return i;
}

ValueTree SampleLibrary::saveState() const
{
    ValueTree vt ("SAMPLE_FOLDERS");
    for (int m = 0; m < numModules; ++m)
    {
        ValueTree c (moduleName (m));
        c.setProperty ("source", sourceKeys[sources[(size_t) m]], nullptr);
        c.setProperty ("user", userFolders[(size_t) m].getFullPathName(), nullptr);
        vt.appendChild (c, nullptr);
    }
    return vt;
}

void SampleLibrary::loadState (const ValueTree& vt)
{
    if (! vt.isValid()) return;
    for (int m = 0; m < numModules; ++m)
    {
        auto c = vt.getChildWithName (moduleName (m));
        if (! c.isValid()) continue;
        const String src = c.getProperty ("source", "twinbox");
        sources[(size_t) m] = src == "legacy" ? legacy : (src == "user" ? user : twinbox);
        const String up = c.getProperty ("user");
        userFolders[(size_t) m] = up.isNotEmpty() ? File (up) : File();
    }
    reloadAll();
}

//==============================================================================
//  LegacyImporter
//==============================================================================
LegacyImporter::Result LegacyImporter::run (const File& ens, std::function<void (float)> progress)
{
    Result res;
    MemoryBlock mb;
    if (! ens.loadFileAsData (mb) || mb.getSize() < 1024) { res.message = "Cannot read the file."; return res; }
    const auto* d = static_cast<const uint8_t*> (mb.getData());
    const size_t n = mb.getSize();
    auto u32 = [d, n] (size_t p) -> uint32_t { return p + 4 <= n ? (uint32_t) (d[p] | (d[p + 1] << 8) | (d[p + 2] << 16) | ((uint32_t) d[p + 3] << 24)) : 0u; };
    auto find = [d, n] (const char* pat, size_t from) -> size_t
    {
        const size_t len = strlen (pat);
        for (size_t i = from; i + len <= n; ++i)
            if (d[i] == (uint8_t) pat[0] && memcmp (d + i, pat, len) == 0) return i;
        return n;
    };

    struct Entry { String name; size_t data; uint32_t frames, ch, sr; };
    struct Map { String name; std::vector<Entry> entries; };
    std::vector<Map> maps;

    std::vector<size_t> mapPos;
    for (size_t p = find ("NIMapFile", 0); p < n; p = find ("NIMapFile", p + 9)) mapPos.push_back (p);
    if (mapPos.empty()) { res.message = "No sample maps found: is this the Limelite ensemble?"; return res; }

    for (size_t mi = 0; mi < mapPos.size(); ++mi)
    {
        const size_t start = mapPos[mi], end = mi + 1 < mapPos.size() ? mapPos[mi + 1] : n;
        Map map;
        // map name = last printable run before the marker
        {
            String last, cur;
            for (size_t i = start > 80 ? start - 80 : 0; i < start; ++i)
            {
                if (d[i] >= 0x20 && d[i] < 0x7f) cur += (juce_wchar) d[i];
                else { if (cur.length() >= 3) last = cur; cur.clear(); }
            }
            if (cur.length() >= 3) last = cur;
            map.name = last.trim();
        }
        for (size_t p = find ("file:///", start); p < end; p = find ("file:///", p + 8))
        {
            size_t q = p;
            while (q < end && d[q] != 0) ++q;
            const size_t lenPos = q + 1;
            const uint32_t chars = u32 (lenPos);
            if (chars == 0 || chars > 300) continue;
            const size_t h = lenPos + 4 + chars * 2;
            if (u32 (h) == 2) continue;                       // first (reference) occurrence
            const uint32_t bytes = u32 (h), frames = u32 (h + 4), ch = u32 (h + 8), sr = u32 (h + 12), bits = u32 (h + 20);
            if ((ch != 1 && ch != 2) || bits != 32 || sr < 8000 || sr > 192000) continue;
            if ((size_t) frames * ch * 4 > bytes || h + 28 + (size_t) bytes > n) continue;
            String path = String::fromUTF8 ((const char*) d + p, (int) (q - p));
            Entry e { URL::removeEscapeChars (path.fromLastOccurrenceOf ("/", false, false)).upToLastOccurrenceOf (".", false, true),
                      h + 28, frames, ch, sr };
            map.entries.push_back (e);
        }
        if (! map.entries.empty()) maps.push_back (map);
    }

    auto findMap = [&maps] (const String& nm) -> const Map*
    {
        for (auto& m : maps) if (m.name.equalsIgnoreCase (nm)) return &m;
        return nullptr;
    };
    struct Target { const char* map; int module; bool open; };
    const Target targets[] { { "Kick", SampleLibrary::kick, false }, { "Snare", SampleLibrary::snare, false },
                             { "Closed Hat", SampleLibrary::hat, false }, { "Open Hat", SampleLibrary::hat, true },
                             { "Tie", SampleLibrary::tie, false }, { "Pod", SampleLibrary::pod, false } };

    int total = 0, done = 0;
    for (auto& t : targets) if (auto* m = findMap (t.map)) total += (int) m->entries.size();
    if (total == 0) { res.message = "The sample maps of Limelite were not found in this file."; return res; }

    const Map* closedHat = findMap ("Closed Hat");
    WavAudioFormat wav;
    for (auto& t : targets)
    {
        const Map* m = findMap (t.map);
        if (m == nullptr) continue;
        File folder = SampleLibrary::folderFor (t.module, SampleLibrary::legacy);
        if (t.open) folder = folder.getChildFile ("open");
        folder.createDirectory();
        for (size_t i = 0; i < m->entries.size(); ++i)
        {
            const auto& e = m->entries[i];
            // open hats take the name of the closed hat with the same index so that they pair up
            String base = e.name;
            if (t.open && closedHat != nullptr && i < closedHat->entries.size()) base = closedHat->entries[i].name;
            const String fname = String (i + 1).paddedLeft ('0', 3) + " " + File::createLegalFileName (base) + ".wav";
            AudioBuffer<float> buf ((int) e.ch, (int) e.frames);
            for (uint32_t c = 0; c < e.ch; ++c)                    // planar float32 data
                memcpy (buf.getWritePointer ((int) c), d + e.data + (size_t) c * e.frames * 4, (size_t) e.frames * 4);
            auto out = folder.getChildFile (fname);
            out.deleteFile();
            if (auto os = std::unique_ptr<FileOutputStream> (out.createOutputStream()))
            {
                std::unique_ptr<AudioFormatWriter> w (wav.createWriterFor (os.get(), e.sr, e.ch, 24, {}, 0));
                if (w != nullptr)
                {
                    os.release();
                    w->writeFromAudioSampleBuffer (buf, 0, buf.getNumSamples());
                    ++res.written;
                }
            }
            ++done;
            if (progress) progress ((float) done / (float) total);
        }
    }
    res.ok = res.written > 0;
    res.message = res.ok ? String (res.written) + " samples imported into the Legacy folders."
                         : String ("No samples could be written.");
    return res;
}
