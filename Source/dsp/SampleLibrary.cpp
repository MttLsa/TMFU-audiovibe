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
        if (files.size() > 128) files.removeRange (128, files.size() - 128);   // max 128 samples per module
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
        // pitch cache: "<file>\t<size>\t<pitch>\t<conf>" lines in a hidden file inside the folder
        const auto cacheFile = folder.getChildFile (".twinbox_pitch.txt");
        std::map<String, std::pair<String, std::pair<float, float>>> cache;
        {
            StringArray lines; lines.addLines (cacheFile.loadFileAsString());
            for (auto& l : lines)
            {
                StringArray t; t.addTokens (l, "\t", {});
                if (t.size() == 4) cache[t[0]] = { t[1], { t[2].getFloatValue(), t[3].getFloatValue() } };
            }
        }
        StringArray newCache;
        bool cacheDirty = false;
        for (auto& f : audioFilesIn (folder))
        {
            if (auto s = loadFile (fm, f))
            {
                {
                    auto* ms = const_cast<Sample*> (s.get());
                    const String sizeKey = String (f.getSize());
                    auto it = cache.find (f.getFileName());
                    if (it != cache.end() && it->second.first == sizeKey) { ms->pitch = it->second.second.first; ms->pitchConf = it->second.second.second; }
                    else { ms->pitch = detectPitch (*ms, ms->pitchConf); cacheDirty = true; }
                    ms->namePitch = pitchFromName (ms->name, ms->pitch);
                    newCache.add (f.getFileName() + "\t" + sizeKey + "\t" + String (ms->pitch, 3) + "\t" + String (ms->pitchConf, 3));
                }
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
        if (cacheDirty) cacheFile.replaceWithText (newCache.joinIntoString ("\n"));   // silently ignored if read-only
        set->offsets = std::vector<std::atomic<float>> ((size_t) set->size());
        bool current;
        {
            const ScopedLock sl (lock);
            current = generation[(size_t) m] == gen;
            for (int i = 0; i < set->size(); ++i)
            {
                auto it = offsetMap.find (folder.getFullPathName() + "|" + set->names[i]);
                set->offsets[(size_t) i].store (it != offsetMap.end() ? it->second : 0.0f);
            }
        }
        if (current)
        {
            std::atomic_store (&sets[(size_t) m], SampleSetPtr (set));
            sendChangeMessage();
        }
        --pending;
    });
}

void SampleLibrary::setSampleOffset (int m, int index, float st)
{
    auto s = set (m);
    if (index < 0 || index >= s->size()) return;
    st = jlimit (-32.0f, 32.0f, st);
    s->offsets[(size_t) index].store (st);
    const ScopedLock sl (lock);
    const String key = s->folder.getFullPathName() + "|" + s->names[index];
    if (std::abs (st) < 1.0e-4f) offsetMap.erase (key); else offsetMap[key] = st;
}

float SampleLibrary::sampleOffset (int m, int index) const
{
    auto s = set (m);
    return index >= 0 && index < s->size() ? s->offsets[(size_t) index].load() : 0.0f;
}

String SampleLibrary::noteName (float midi)
{
    if (midi < 0) return "-";
    static const char* n[] { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const int r = roundToInt (midi);
    const int cents = roundToInt ((midi - (float) r) * 100.0f);
    return String (n[((r % 12) + 12) % 12]) + String (r / 12 - 2) + (cents == 0 ? String() : (cents > 0 ? " +" : " ") + String (cents) + "c");
}

float SampleLibrary::pitchFromName (const String& fileName, float nearPitch)
{
    // split into tokens on separators and camel-case / letter-digit boundaries are kept inside tokens
    StringArray tokens;
    tokens.addTokens (fileName.replaceCharacters ("_-.,()[]{}+&'", "             "), " ", "");
    tokens.removeEmptyStrings();

    auto qualityOk = [] (String rest) -> bool
    {
        // allowed chord-quality / mode suffixes after the root ("m", "min", "maj7", "sus4", "add9", "dim", "aug", "7", "m9", ...)
        static const char* words[] { "minor", "minore", "major", "maggiore", "magg", "maj", "min", "dim", "aug", "sus", "add", "m", "M" };
        while (rest.isNotEmpty())
        {
            bool matched = false;
            for (auto* w : words)
                if (rest.startsWith (w)) { rest = rest.substring ((int) strlen (w)); matched = true; break; }
            if (! matched)
            {
                if (CharacterFunctions::isDigit (rest[0])) { rest = rest.substring (1); matched = true; }
                else if (rest[0] == '#' || rest[0] == 'b' || rest[0] == 'o') { rest = rest.substring (1); matched = true; }
            }
            if (! matched) return false;
        }
        return true;
    };

    float best = -1.0f;
    for (int ti = tokens.size(); --ti >= 0;)   // the key is usually at the end of the name
    {
        String t = tokens[ti].trim();
        if (t.isEmpty()) continue;
        int pc = -1; int len = 0;
        // solfege (Do Re Mi Fa Sol La Si), case-insensitive
        static const std::pair<const char*, int> solf[] { { "sol", 7 }, { "do", 0 }, { "re", 2 }, { "mi", 4 }, { "fa", 5 }, { "la", 9 }, { "si", 11 } };
        for (auto& sf : solf)
            if (t.startsWithIgnoreCase (sf.first) && (t.length() == (int) strlen (sf.first) || ! CharacterFunctions::isLetter (t[(int) strlen (sf.first)])
                                                       || t.substring ((int) strlen (sf.first)).startsWithIgnoreCase ("m")))
            { pc = sf.second; len = (int) strlen (sf.first); break; }
        // letter names A-G: uppercase, or lowercase only when followed by an accidental / digit / quality
        if (pc < 0)
        {
            const juce_wchar c = t[0];
            const juce_wchar up = CharacterFunctions::toUpperCase (c);
            if (up >= 'A' && up <= 'G' && (c == up || t.length() > 1))
            {
                static const int base[] { 9, 11, 0, 2, 4, 5, 7 };   // A B C D E F G
                pc = base[up - 'A']; len = 1;
                if (c != up && t.length() > 1 && ! (t[1] == '#' || t[1] == 'b' || CharacterFunctions::isDigit (t[1]) || t[1] == 'm')) pc = -1;
            }
        }
        if (pc < 0) continue;
        String rest = t.substring (len);
        // accidental
        if (rest.startsWith ("#") || rest.startsWithIgnoreCase ("sharp") || rest.startsWith (String::fromUTF8 ("\xe2\x99\xaf")))
        { pc += 1; rest = rest.startsWith ("#") ? rest.substring (1) : (rest.startsWithIgnoreCase ("sharp") ? rest.substring (5) : rest.substring (1)); }
        else if (rest.startsWith ("b") && ! rest.startsWith ("bass") && ! rest.startsWith ("bpm")) { pc -= 1; rest = rest.substring (1); }
        else if (rest.startsWithIgnoreCase ("flat")) { pc -= 1; rest = rest.substring (4); }
        else if (rest.startsWith (String::fromUTF8 ("\xe2\x99\xad"))) { pc -= 1; rest = rest.substring (1); }
        pc = (pc + 12) % 12;
        // octave: a plain number (C3 = 60, Ableton convention) - "C7"-style numbers > 8 are chord extensions
        int octave = -100;
        if (rest.isNotEmpty() && (rest.containsOnly ("-0123456789")))
        {
            const int o = rest.getIntValue();
            if (o >= -2 && o <= 8 && rest.length() <= 2) { octave = o; rest.clear(); }
        }
        // a following word may carry the quality ("Re minore", "A minor")
        if (rest.isNotEmpty() && ! qualityOk (rest)) continue;
        if (octave > -100) best = (float) ((octave + 2) * 12 + pc);
        else
        {
            const float ref = nearPitch >= 0.0f ? nearPitch : 60.0f;
            float n = (float) pc + 12.0f * std::round ((ref - (float) pc) / 12.0f);
            best = n;
        }
        break;
    }
    return best;
}

// YIN fundamental estimation on a few windows after the attack; median of the confident estimates
float SampleLibrary::detectPitch (const Sample& s, float& conf)
{
    conf = 0.0f;
    const auto& L = s.L;
    // mono mix, decimated to ~22 kHz (enough for fundamentals up to 2 kHz, 4x faster analysis)
    const int dec = s.sourceRate > 30000.0 ? (s.sourceRate > 60000.0 ? 4 : 2) : 1;
    const double sr = s.sourceRate / dec;
    const int n = (int) L.size() / dec;
    if (n < 1024) return -1.0f;
    std::vector<float> x ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        float acc = 0;
        for (int k = 0; k < dec; ++k)
        {
            const size_t j = (size_t) (i * dec + k);
            acc += s.R.empty() ? L[j] : 0.5f * (L[j] + s.R[j]);
        }
        x[(size_t) i] = acc / (float) dec;
    }
    const int tauMin = jmax (2, (int) (sr / 2000.0)), tauMax = jmin (n / 3, (int) (sr / 30.0));
    const int W = jmin (2048, n - tauMax - 1);
    if (W < 256 || tauMax <= tauMin + 2) return -1.0f;
    // start after the attack (peak + 15 ms)
    int peak = 0; float pk = 0;
    for (int i = 0; i < jmin (n, (int) (sr * 0.2)); ++i) if (std::abs (x[(size_t) i]) > pk) { pk = std::abs (x[(size_t) i]); peak = i; }
    if (pk < 1.0e-4f) return -1.0f;
    std::vector<float> estimates, confs;
    std::vector<double> d ((size_t) tauMax + 1);
    for (int w = 0; w < 3; ++w)
    {
        const int start = peak + (int) (sr * 0.015) + w * (W / 2);
        if (start + W + tauMax >= n) break;
        double energy = 0; for (int i = 0; i < W; ++i) energy += (double) x[(size_t) (start + i)] * x[(size_t) (start + i)];
        if (energy / W < 1.0e-7) break;
        d[0] = 1.0; double run = 0;
        for (int tau = 1; tau <= tauMax; ++tau)
        {
            double sum = 0;
            const float* a = x.data() + start; const float* b2 = a + tau;
            for (int i = 0; i < W; ++i) { const double df = (double) a[i] - b2[i]; sum += df * df; }
            run += sum;
            d[(size_t) tau] = run > 0 ? sum * tau / run : 1.0;
        }
        int best = -1;
        for (int tau = tauMin; tau < tauMax; ++tau)
            if (d[(size_t) tau] < 0.15)
            {
                while (tau + 1 < tauMax && d[(size_t) tau + 1] < d[(size_t) tau]) ++tau;
                best = tau; break;
            }
        if (best < 0) continue;
        // parabolic interpolation
        double t = best;
        if (best > 1 && best < tauMax)
        {
            const double a = d[(size_t) best - 1], b3 = d[(size_t) best], c = d[(size_t) best + 1];
            const double den = a - 2 * b3 + c;
            if (std::abs (den) > 1e-12) t = best + 0.5 * (a - c) / den;
        }
        const double f = sr / t;
        estimates.push_back ((float) (69.0 + 12.0 * std::log2 (f / 440.0)));
        confs.push_back ((float) (1.0 - d[(size_t) best]));
    }
    if (estimates.empty()) return -1.0f;
    std::vector<float> e = estimates; std::sort (e.begin(), e.end());
    const float med = e[e.size() / 2];
    // consistency check: estimates must agree within half a semitone
    int agree = 0; float c = 0;
    for (size_t i = 0; i < estimates.size(); ++i) if (std::abs (estimates[i] - med) < 0.5f) { ++agree; c += confs[i]; }
    if (agree * 2 < (int) estimates.size() + 1) return -1.0f;
    conf = c / (float) agree;
    return med;
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
    {
        const ScopedLock sl (lock);
        StringArray o;
        for (auto& kv : offsetMap) o.add (kv.first + "=" + String (kv.second, 3));
        vt.setProperty ("offsets", o.joinIntoString ("\n"), nullptr);
    }
    return vt;
}

void SampleLibrary::loadState (const ValueTree& vt)
{
    if (! vt.isValid()) return;
    {
        const ScopedLock sl (lock);
        offsetMap.clear();
        StringArray o; o.addLines (vt.getProperty ("offsets").toString());
        for (auto& l : o)
            if (l.containsChar ('=')) offsetMap[l.upToLastOccurrenceOf ("=", false, false)] = l.fromLastOccurrenceOf ("=", false, false).getFloatValue();
    }
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
