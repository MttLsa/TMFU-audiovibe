#include "PatternImport.h"
#include "PluginProcessor.h"

using namespace juce;
using namespace params;

// ================================================================================================= parser
const PatternImport::Node* PatternImport::Node::child (const String& k) const
{
    for (auto* c : children) if (c->key.equalsIgnoreCase (k)) return c;
    return nullptr;
}

namespace
{
    struct Reader
    {
        const String& t; int i = 0;
        juce_wchar peek() const { return i < t.length() ? t[i] : 0; }
        bool done() const { return i >= t.length(); }
        void skipSpace()
        {
            for (;;)
            {
                while (! done() && (CharacterFunctions::isWhitespace (peek()) || peek() == ',' || peek() == ';')) ++i;
                if (t.substring (i, i + 2) == "/*") { const int e = t.indexOf (i + 2, "*/"); i = e < 0 ? t.length() : e + 2; continue; }
                if (t.substring (i, i + 2) == "//") { while (! done() && peek() != '\n') ++i; continue; }
                break;
            }
        }
    };

    void parseBlock (Reader& r, PatternImport::Node& into, int depth)
    {
        while (! r.done())
        {
            r.skipSpace();
            if (r.done()) return;
            if (r.peek() == '}') { ++r.i; return; }
            // key (up to ':' on the same line); a line without ':' is skipped
            const int lineEnd = [&] { const int e = r.t.indexOf (r.i, "\n"); return e < 0 ? r.t.length() : e; }();
            // key separator: ':' ("Key: value") or '=' ("Key=value"), whichever comes first on the line
            int colon = -1;
            for (int k = r.i; k < lineEnd; ++k) if (r.t[k] == ':' || r.t[k] == '=') { colon = k; break; } else if (r.t[k] == '"' || r.t[k] == '{' || r.t[k] == '}') break;
            if (colon < 0)
            {
                // a lone '{' opens an anonymous block; a list ("Off, Off, On }") is stored as values; '}' closes the block
                if (r.peek() == '{') { ++r.i; auto* n = into.children.add (new PatternImport::Node()); if (depth < 16) parseBlock (r, *n, depth + 1); continue; }
                int e = r.i;
                while (e < lineEnd && r.t[e] != '}' && r.t[e] != ',') ++e;
                if (e > r.i) { auto* v = into.children.add (new PatternImport::Node()); v->value = r.t.substring (r.i, e).trim(); }
                r.i = e;
                if (r.i < r.t.length() && r.t[r.i] == '}') { ++r.i; return; }
                if (r.i < r.t.length() && r.t[r.i] == ',') ++r.i;
                continue;
            }
            auto* n = into.children.add (new PatternImport::Node());
            n->key = r.t.substring (r.i, colon).trim().unquoted();
            r.i = colon + 1;
            while (! r.done() && (r.peek() == ' ' || r.peek() == '\t')) ++r.i;
            if (r.peek() == '\n' || r.peek() == '\r') r.skipSpace();
            if (r.peek() == '{') { ++r.i; if (depth < 16) parseBlock (r, *n, depth + 1); continue; }
            if (r.peek() == '"')
            {
                const int e = r.t.indexOf (r.i + 1, "\"");
                n->value = r.t.substring (r.i + 1, e < 0 ? r.t.length() : e);
                r.i = e < 0 ? r.t.length() : e + 1;
                continue;
            }
            int e = r.i;
            while (e < r.t.length() && r.t[e] != '\n' && r.t[e] != '}' && r.t[e] != ';' && r.t[e] != ',') ++e;
            n->value = r.t.substring (r.i, e).trim();
            r.i = e;
        }
    }

    // ---- values with units ("49.26Hz", "1.2kHz", "316.22ms", "0.3s", "+24.96sm", "-3.5dB", "60%")
    double number (const String& s, String& unit)
    {
        const String v = s.trim();
        int k = 0;
        while (k < v.length() && (CharacterFunctions::isDigit (v[k]) || v[k] == '.' || v[k] == '-' || v[k] == '+' || (k > 0 && (v[k] == 'e' || v[k] == 'E') && k + 1 < v.length() && (CharacterFunctions::isDigit (v[k + 1]) || v[k + 1] == '-')))) ++k;
        unit = v.substring (k).trim().toLowerCase();
        return v.substring (0, k).getDoubleValue();
    }
    double hz (const String& s) { String u; const double v = number (s, u); return u.startsWith ("k") ? v * 1000.0 : v; }
    double ms (const String& s) { String u; const double v = number (s, u); return (u == "s" || u == "sec") ? v * 1000.0 : v; }
    double num (const String& s) { String u; return number (s, u); }
    bool onOff (const String& s) { const auto v = s.trim().toLowerCase(); return v == "on" || v == "1" || v == "true" || v == "yes"; }
    int choice (const String& s, std::initializer_list<const char*> names, int def)
    {
        const auto v = s.trim().toLowerCase();
        int idx = 0;
        for (auto* n : names) { if (v.startsWith (String (n).toLowerCase())) return idx; ++idx; }
        return def;
    }
    int digitsIn (const String& k) { const auto d = k.retainCharacters ("0123456789"); return d.isEmpty() ? -1 : d.getIntValue(); }

    bool isPatch (const PatternImport::Node& n)
    {
        for (auto* k : { "OscFreq", "OscWave", "OscDcy", "NFilFrq", "NEnvDcy", "ModMode" }) if (n.child (k) != nullptr) return true;
        return false;
    }
    const PatternImport::Node* channelRows (const PatternImport::Node& n, int ch)   // channel block 1..8 of a pattern
    {
        for (auto* c : n.children)
            if (c->children.size() > 0 && digitsIn (c->key) == ch + 1
                && (c->child ("Triggers") || c->child ("Trigger") || c->child ("Trigs") || c->child ("Accents") || c->child ("Fills"))) return c;
        return nullptr;
    }
    bool isPattern (const PatternImport::Node& n) { for (int ch = 0; ch < 8; ++ch) if (channelRows (n, ch)) return true; return false; }

    bool stepOn (juce_wchar c) { return ! (c == '-' || c == '.' || c == '0' || c == ' ' || c == '_' || c == 0); }

    void setNatural (AudioProcessorValueTreeState& s, const String& id, float value)
    {
        if (auto* prm = s.getParameter (id))
        {
            prm->beginChangeGesture();
            prm->setValueNotifyingHost (prm->convertTo0to1 (prm->getNormalisableRange().snapToLegalValue (value)));
            prm->endChangeGesture();
        }
    }

    // ---- one drum patch -> channel ch
    void applyPatch (TwinboxProcessor& p, const PatternImport::Node& n, int ch)
    {
        auto& s = p.apvts;
        auto set = [&] (const char* key, const char* id, std::function<float (const String&)> conv)
        { if (auto* c = n.child (key)) if (c->value.isNotEmpty()) setNatural (s, a (ch, id), conv (c->value)); };
        if (auto nm = n.get ("Name"); nm.isNotEmpty()) p.setChannelName (ch, nm.substring (0, 24));
        set ("OscWave", mt::oscWave, [] (const String& v) { return (float) choice (v, { "Sine", "Tri", "Saw" }, 0); });
        set ("OscFreq", mt::oscFreq, [] (const String& v) { return (float) hz (v); });
        set ("OscAtk",  mt::oscAtk,  [] (const String& v) { return (float) ms (v); });
        set ("OscDcy",  mt::oscDcy,  [] (const String& v) { return (float) ms (v); });
        const int mode = choice (n.get ("ModMode"), { "Decay", "Sine", "Noise" }, 0);
        if (n.child ("ModMode")) setNatural (s, a (ch, mt::pmMode), (float) mode);
        set ("ModAmt", mt::pmAmt, [] (const String& v) { return (float) num (v); });
        // Mod Rate -> 0..100 % of the DUALBOX mapping: Decay tau 1 ms..10 s, Sine 0.1 Hz..20 kHz, Random 1 Hz..20 kHz
        set ("ModRate", mt::pmRate, [mode] (const String& v)
        {
            String u; double x = number (v, u);
            double r;
            if (mode == 0) { if (u == "s") x *= 1000.0; if (u.contains ("hz")) x = 1000.0 / jmax (1.0e-3, x * (u.startsWith ("k") ? 1000.0 : 1.0)); r = std::log10 (jmax (1.0, x)) / 4.0; }
            else
            {
                if (u.startsWith ("k")) x *= 1000.0;
                if (u == "ms" || u == "s") x = 1000.0 / jmax (1.0e-3, u == "s" ? x * 1000.0 : x);
                r = mode == 1 ? std::log (jmax (0.1, x) / 0.1) / std::log (200000.0) : std::log (jmax (1.0, x)) / std::log (20000.0);
            }
            return (float) jlimit (0.0, 100.0, r * 100.0);
        });
        auto filt = [] (const String& v) { return (float) choice (v, { "LP", "BP", "HP" }, 0); };
        set ("NFilMode", mt::nMode, filt);
        set ("NFilMod",  mt::nMode, filt);
        set ("NFilFrq",  mt::nFreq, [] (const String& v) { return (float) hz (v); });
        set ("NFilQ",    mt::nQ,    [] (const String& v) { return (float) num (v); });
        set ("NStereo",  mt::nStereo, [] (const String& v) { return onOff (v) ? 1.0f : 0.0f; });
        auto env = [] (const String& v) { return (float) choice (v, { "Exp", "Lin", "Mod" }, 0); };
        set ("NEnvMode", mt::nEnv, env);
        set ("NEnvMod",  mt::nEnv, env);
        set ("NEnvAtk",  mt::nAtk,  [] (const String& v) { return (float) ms (v); });
        set ("NEnvDcy",  mt::nDcy,  [] (const String& v) { return (float) ms (v); });
        // Mix "osc/noise" (e.g. 82.00/18.00) -> noise share 0..100
        set ("Mix", mt::mix, [] (const String& v)
        {
            if (v.containsChar ('/'))
            {
                const double o = v.upToFirstOccurrenceOf ("/", false, false).getDoubleValue(), nz = v.fromFirstOccurrenceOf ("/", false, false).getDoubleValue();
                return (float) (o + nz > 0 ? nz / (o + nz) * 100.0 : 50.0);
            }
            return (float) num (v);
        });
        set ("DistAmt", mt::dist,   [] (const String& v) { return (float) num (v); });
        set ("EQFreq",  mt::eqFreq, [] (const String& v) { return (float) hz (v); });
        set ("EQGain",  mt::eqGain, [] (const String& v) { return (float) num (v); });
        set ("Level",   mt::level,  [] (const String& v) { return (float) num (v); });
        set ("Pan",     mt::pan,    [] (const String& v) { return (float) num (v); });
        set ("Choke",   mt::choke,  [] (const String& v) { return onOff (v) ? 1.0f : 0.0f; });
        set ("OscVel",  mt::velOsc, [] (const String& v) { return (float) num (v); });
        set ("NVel",    mt::velNoise, [] (const String& v) { return (float) num (v); });
        set ("ModVel",  mt::velMod, [] (const String& v) { return (float) num (v); });
        // loudness: the source synth has a different gain structure (EQ up to +40 dB, levels up to +10 dB);
        // channels that would be louder than the default kit are turned down to its loudest channel
        const float peakDb = Decibels::gainToDecibels (PatternImport::channelPeak (p, ch), -100.0f);
        if (peakDb > PatternImport::maxChannelPeakDb)
            setNatural (s, a (ch, mt::level), p.apvts.getRawParameterValue (a (ch, mt::level))->load() - (peakDb - PatternImport::maxChannelPeakDb));
    }

    // ---- one pattern -> slot
    void applyPattern (TwinboxProcessor& p, const PatternImport::Node& n, int slot)
    {
        auto& P = p.micro.pattern (slot);
        P.clear();
        auto row = [] (const PatternImport::Node* c, std::initializer_list<const char*> keys) -> String
        { if (c != nullptr) for (auto* k : keys) if (auto* r = c->child (k)) return r->value; return {}; };
        int longest = 0;
        const String globalAcc = row (&n, { "Accents", "Accent" });
        for (int ch = 0; ch < MicroEngine::NC; ++ch)
        {
            auto* c = channelRows (n, ch);
            const String tr = row (c, { "Triggers", "Trigger", "Trigs" }), ac = row (c, { "Accents", "Accent" }), fl = row (c, { "Fills", "Fill" });
            longest = jmax (longest, tr.length(), fl.length());
            for (int st = 0; st < MicroEngine::NS; ++st)
            {
                uint8_t f = 0;
                const bool t = st < tr.length() && stepOn (tr[st]);
                if (t) f |= MicroEngine::trig;
                if (t && ((st < ac.length() && stepOn (ac[st])) || (st < globalAcc.length() && stepOn (globalAcc[st])) || (st < tr.length() && CharacterFunctions::isUpperCase (tr[st]) && tr[st] != 'O'))) f |= MicroEngine::accent;
                if (st < fl.length() && stepOn (fl[st])) f |= MicroEngine::fill | MicroEngine::trig;
                P.steps[(size_t) ch][(size_t) st].store (f);
            }
        }
        int len = n.get ("Length").getIntValue();
        if (len <= 0) len = n.get ("Steps").getIntValue();
        if (len <= 0) len = longest > 0 ? longest : 16;
        P.length.store (jlimit (1, MicroEngine::NS, len));
    }

    int patternSlot (const String& key, int fallback)
    {
        const auto k = key.trim().toLowerCase();
        if (k.length() == 1 && k[0] >= 'a' && k[0] <= 'l') return (int) (k[0] - 'a');
        const int d = digitsIn (k);
        if (d >= 1 && d <= MicroEngine::NP) return d - 1;
        return fallback;
    }
}

float PatternImport::channelPeak (TwinboxProcessor& p, int ch)
{
    auto v = [&] (const char* id) { return p.apvts.getRawParameterValue (a (ch, id))->load(); };
    MicroPatch m;
    m.wave = (int) v (mt::oscWave); m.freq = v (mt::oscFreq); m.atkMs = v (mt::oscAtk); m.dcyMs = v (mt::oscDcy);
    m.pmMode = (int) v (mt::pmMode); m.pmAmt = v (mt::pmAmt); m.pmRate = v (mt::pmRate);
    m.nMode = (int) v (mt::nMode); m.nFreq = v (mt::nFreq); m.nQ = v (mt::nQ); m.stereo = v (mt::nStereo) > 0.5f; m.nEnv = (int) v (mt::nEnv);
    m.nAtkMs = v (mt::nAtk); m.nDcyMs = v (mt::nDcy);
    m.mix = v (mt::mix); m.dist = v (mt::dist); m.eqFreq = v (mt::eqFreq); m.eqGain = v (mt::eqGain); m.levelDb = v (mt::level); m.pan = v (mt::pan);
    m.velOsc = v (mt::velOsc); m.velNoise = v (mt::velNoise); m.velMod = v (mt::velMod);
    constexpr double sr = 48000.0; constexpr int bs = 512;
    MicroVoice voice; voice.prepare (sr); voice.updateFilters (m); voice.trigger (m, 1.0f);
    std::vector<float> L (bs), R (bs);
    float peak = 0.0f;
    for (int pos = 0; pos < (int) (sr * 4.0) && voice.isActive(); pos += bs)
    {
        std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f);
        voice.render (m, L.data(), R.data(), bs);
        for (int i = 0; i < bs; ++i) peak = jmax (peak, std::abs (L[(size_t) i]), std::abs (R[(size_t) i]));
    }
    return peak;
}

std::unique_ptr<PatternImport::Node> PatternImport::parse (const String& text)
{
    auto root = std::make_unique<Node>();
    Reader r { text };
    parseBlock (r, *root, 0);
    return root;
}

// ================================================================================================= import
PatternImport::Result PatternImport::run (TwinboxProcessor& p, const File& file, int targetPattern, int channel)
{
    if (! file.existsAsFile()) return { false, 0, 0, "File not found." };
    if (file.getSize() > 4 * 1024 * 1024) return { false, 0, 0, "This file is too large to be a pattern file." };
    return runText (p, file.loadFileAsString(), targetPattern, channel);
}

PatternImport::Result PatternImport::runText (TwinboxProcessor& p, const String& text, int targetPattern, int channel)
{
    Result res;
    auto root = parse (text);
    // unwrap single top-level blocks ("Preset: { ... }", "Pattern: { ... }", "{ ... }")
    const Node* top = root.get();
    while (top->children.size() == 1 && top->children[0]->children.size() > 0 && ! isPatch (*top) && ! isPattern (*top)
           && top->child ("Patterns") == nullptr && top->child ("DrumPatches") == nullptr)
        top = top->children[0];

    // ---- drum patches
    const Node* patches = nullptr;
    for (auto* k : { "DrumPatches", "Patches", "Drums", "Patch" }) if ((patches = top->child (k)) != nullptr && patches->children.size() > 0) break; else patches = nullptr;
    if (patches != nullptr && isPatch (*patches)) { applyPatch (p, *patches, channel); res.patches = 1; }
    else if (patches != nullptr)
    {
        int idx = 0;
        for (auto* c : patches->children)
        {
            if (! isPatch (*c)) continue;
            const int d = digitsIn (c->key);
            const int ch = (d >= 1 && d <= MicroEngine::NC) ? d - 1 : idx;
            if (ch < MicroEngine::NC) { applyPatch (p, *c, ch); ++res.patches; }
            ++idx;
        }
    }
    else if (isPatch (*top)) { applyPatch (p, *top, channel); res.patches = 1; }

    // ---- patterns
    if (auto* pats = top->child ("Patterns"))
    {
        int idx = 0;
        for (auto* c : pats->children)
        {
            if (! isPattern (*c)) { ++idx; continue; }
            const int slot = patternSlot (c->key, idx);
            if (slot >= 0 && slot < MicroEngine::NP) { applyPattern (p, *c, slot); ++res.patterns; }
            ++idx;
        }
    }
    else if (auto* one = top->child ("Pattern"); one != nullptr && isPattern (*one)) { applyPattern (p, *one, targetPattern); res.patterns = 1; }
    else if (isPattern (*top)) { applyPattern (p, *top, targetPattern); res.patterns = 1; }

    if (res.patterns > 1)   // preset: swing, fill rate, selected pattern
    {
        if (auto sw = top->get ("Swing"); sw.isNotEmpty()) setNatural (p.apvts, aSwing, (float) num (sw));
        if (auto fr = top->get ("FillRate"); fr.isNotEmpty())
        {
            const int x = (int) std::lround (num (fr));
            setNatural (p.apvts, aFillRate, (float) (x >= 2 && x <= 8 ? x - 1 : 0));   // Manual, 2 .. 8
        }
        if (auto* pn = top->child ("Pattern"); pn != nullptr && pn->children.isEmpty())
        {
            const int slot = patternSlot (pn->value, -1);
            if (slot >= 0) setNatural (p.apvts, aPattern, (float) slot);
        }
    }
    if (res.patches > 1)
        if (auto* mutes = top->child ("Mutes"))
            for (int ch = 0; ch < jmin (MicroEngine::NC, mutes->children.size()); ++ch)
                setNatural (p.apvts, a (ch, mt::mute), onOff (mutes->children[ch]->value) ? 1.0f : 0.0f);

    res.ok = res.patterns > 0 || res.patches > 0;
    res.message = res.ok ? "Imported " + String (res.patterns) + " pattern(s) and " + String (res.patches) + " drum patch(es)."
                         : String ("No patterns or drum patches found in this file.");
    return res;
}
