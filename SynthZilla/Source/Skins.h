#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// =====================================================================================================
//  Panel skins. When a preset is loaded each section takes the look of the instrument the original
//  sound came from (colours, panel material, kind of controls, display). They are evocative homages:
//  no logos, artwork or trade dress of the original manufacturers are reproduced.
// =====================================================================================================
namespace skins
{
    enum KnobStyle { Modern = 0, Skirt, Pointer, CapKnob, Fader, Membrane };
    enum Pattern { None = 0, Stripes, PatchBay, Rack, Brushed, Crt, PinMatrix };

    struct Skin
    {
        const char* key;
        juce::Colour body, body2, text, label, accent, knob, cap;
        KnobStyle knobStyle;
        bool wood;
        Pattern pattern;
        bool lcd;
        juce::Colour lcdBg, lcdText;
        juce::Colour stripes[4];
    };

    #define C(x) juce::Colour (0xff##x)
    inline const std::vector<Skin>& all()
    {
        static const std::vector<Skin> s = {
            // key            body       body2      text       label      accent     knob       cap        style     wood   pattern    lcd    lcdBg      lcdText    stripes
            { "neutral-a",    C(2a2a2a), C(333333), C(d0d0d0), C(8c8c8c), C(ff8a3d), C(3a3a3a), C(ff8a3d), Modern,  false, None,     false, C(000000), C(000000), { C(ff8a3d), C(ff8a3d), C(ff8a3d), C(ff8a3d) } },
            { "neutral-d",    C(2a2a2a), C(333333), C(d0d0d0), C(8c8c8c), C(35d0ff), C(3a3a3a), C(35d0ff), Modern,  false, None,     false, C(000000), C(000000), { C(35d0ff), C(35d0ff), C(35d0ff), C(35d0ff) } },
            { "neutral-g",    C(2a2a2a), C(333333), C(d0d0d0), C(8c8c8c), C(eeeeee), C(3a3a3a), C(eeeeee), Modern,  false, None,     false, C(000000), C(000000), { C(eeeeee), C(eeeeee), C(eeeeee), C(eeeeee) } },
            // ---------------------------------------------------------------------------------- analog
            { "minimoog",     C(1c1c1c), C(262626), C(f2f2f2), C(c9c9c9), C(e07b2a), C(101010), C(b9bcc0), Skirt,   true,  None,     false, C(000000), C(000000), { C(e07b2a), C(2f6fd6), C(f2f2f2), C(e07b2a) } },
            { "moog-modular", C(242424), C(2e2e2e), C(efefef), C(bdbdbd), C(d0d0d0), C(0e0e0e), C(c8c8c8), Skirt,   true,  PatchBay, false, C(000000), C(000000), { C(d0d0d0), C(d0d0d0), C(d0d0d0), C(d0d0d0) } },
            { "prophet",      C(141414), C(1d1d1d), C(ececec), C(bfbfbf), C(e03a2f), C(0c0c0c), C(9fa3a8), Pointer, true,  None,     false, C(000000), C(000000), { C(e03a2f), C(e03a2f), C(e03a2f), C(e03a2f) } },
            { "oberheim",     C(191a1c), C(232427), C(f0f0f0), C(c4c7cc), C(3a78dc), C(0d0d0d), C(e8e8e8), Pointer, false, Stripes,  false, C(000000), C(000000), { C(3a78dc), C(3a78dc), C(c4c7cc), C(3a78dc) } },
            { "jupiter",      C(1f1f20), C(29292b), C(f2f2f2), C(c9c9c9), C(ff8c2a), C(141414), C(ff8c2a), CapKnob, false, Stripes,  false, C(000000), C(000000), { C(ff8c2a), C(ffd23a), C(e8402f), C(f2f2f2) } },
            { "juno",         C(3b3c3f), C(46474b), C(f3f3f3), C(d0d0d0), C(e8462e), C(1a1a1a), C(e8e8e8), Fader,   false, Stripes,  false, C(000000), C(000000), { C(e8462e), C(f29a2e), C(f2d23a), C(4a8fd8) } },
            { "alpha-juno",   C(252628), C(2f3033), C(f0f0f0), C(c0c3c8), C(4aa3df), C(121212), C(4aa3df), Fader,   false, Stripes,  false, C(000000), C(000000), { C(4aa3df), C(e8e8e8), C(4aa3df), C(e8e8e8) } },
            { "sh101",        C(8d9095), C(9a9da2), C(141414), C(2a2a2a), C(d8352a), C(1a1a1a), C(e9e9e9), Fader,   false, Stripes,  false, C(000000), C(000000), { C(d8352a), C(3a74c9), C(f2d23a), C(d8352a) } },
            { "tb303",        C(c6c9cc), C(d4d7da), C(1e1e1e), C(333333), C(cc2a1f), C(1b1b1b), C(cfd2d5), CapKnob, false, Brushed,  false, C(000000), C(000000), { C(cc2a1f), C(cc2a1f), C(cc2a1f), C(cc2a1f) } },
            { "ms20",         C(181818), C(222222), C(f2f2f2), C(d9d9d9), C(f2f2f2), C(0d0d0d), C(f2f2f2), Pointer, false, PatchBay, false, C(000000), C(000000), { C(f2f2f2), C(f2f2f2), C(f2f2f2), C(f2f2f2) } },
            { "arp",          C(1a1a1a), C(242424), C(f4f4f4), C(cfcfcf), C(f28c28), C(0f0f0f), C(f28c28), Fader,   false, Stripes,  false, C(000000), C(000000), { C(f28c28), C(f4f4f4), C(f28c28), C(3a3a3a) } },
            { "cs80",         C(3d2a20), C(4a3427), C(f1e6d8), C(d6c4ae), C(d93a2f), C(15100c), C(e8e0d0), Fader,   true,  Stripes,  false, C(000000), C(000000), { C(d93a2f), C(f2f2f2), C(f2c53a), C(3a78c8) } },
            { "ems",          C(e6e1d3), C(d9d3c2), C(1e1e1e), C(3b3b3b), C(2f6fb0), C(1b1b1b), C(2f6fb0), Skirt,   true,  PinMatrix,false, C(000000), C(000000), { C(2f6fb0), C(d9402f), C(e0b02f), C(3b9a4a) } },
            { "polysix",      C(1d1d1d), C(272727), C(efefef), C(c6c6c6), C(e7a23a), C(0e0e0e), C(d8d8d8), Pointer, true,  None,     false, C(000000), C(000000), { C(e7a23a), C(e7a23a), C(e7a23a), C(e7a23a) } },
            { "solina",       C(5d4128), C(6b4b2f), C(f6ead6), C(e2d2b8), C(e8a23c), C(2a1c10), C(f0e2c8), Fader,   true,  None,     false, C(000000), C(000000), { C(e8a23c), C(e8a23c), C(e8a23c), C(e8a23c) } },
            { "modern-analog",C(2b2c2f), C(35363a), C(f3f3f3), C(c3c5c9), C(f5a524), C(141414), C(9da1a7), Skirt,   false, None,     false, C(000000), C(000000), { C(f5a524), C(f5a524), C(f5a524), C(f5a524) } },
            { "mc202",        C(b4b8bc), C(c0c4c8), C(161616), C(2d2d2d), C(3a74c9), C(1a1a1a), C(e6e6e6), Fader,   false, Stripes,  false, C(000000), C(000000), { C(3a74c9), C(d8352a), C(3a74c9), C(d8352a) } },
            // ---------------------------------------------------------------------------------- digital
            { "dx7",          C(2a2421), C(352d29), C(ece4d8), C(c9bdaf), C(2c8f7a), C(1a1513), C(2c8f7a), Membrane,false, Stripes,  true,  C(9cc23a), C(17200a), { C(c43c4a), C(e0a83a), C(3aa38a), C(5c7fbf) } },
            { "tx81z",        C(1e1f21), C(28292c), C(e6e6e6), C(b5b8bc), C(c8c8c8), C(111111), C(bfbfbf), Membrane,false, Rack,     true,  C(8fbf3a), C(14200a), { C(c8c8c8), C(c8c8c8), C(c8c8c8), C(c8c8c8) } },
            { "m1",           C(2d2f33), C(373a3f), C(f2f2f2), C(c2c6cc), C(5aa3e6), C(17181a), C(5aa3e6), Membrane,false, None,     true,  C(a7cf3c), C(14210a), { C(5aa3e6), C(5aa3e6), C(5aa3e6), C(5aa3e6) } },
            { "d50",          C(1c1e21), C(26292d), C(f1f1f1), C(c0c4ca), C(4f8ed8), C(0f1012), C(e8e8e8), Fader,   false, Stripes,  true,  C(98c23c), C(13200a), { C(4f8ed8), C(e8e8e8), C(4f8ed8), C(e8e8e8) } },
            { "jd800",        C(191a1b), C(232425), C(f2f2f2), C(c8c8c8), C(e0453a), C(0e0e0e), C(e0453a), Fader,   false, Stripes,  true,  C(9cc83c), C(13200a), { C(e0453a), C(3a8fd8), C(f2c53a), C(3ab36a) } },
            { "jp8000",       C(5c6168), C(6a6f77), C(f4f4f4), C(d5d8dc), C(e33b2f), C(1a1b1d), C(c9ccd0), Pointer, false, None,     true,  C(e33b2f), C(1a0303), { C(e33b2f), C(3a8fd8), C(e33b2f), C(3a8fd8) } },
            { "cz",           C(2a2b2f), C(34363a), C(f0f0f0), C(c4c7cc), C(ff7b2b), C(17181a), C(ff7b2b), Membrane,false, Stripes,  true,  C(a3c93c), C(13200a), { C(ff7b2b), C(3a8fd6), C(f2d23a), C(ff7b2b) } },
            { "ppg",          C(1f3f78), C(274a88), C(f2f5fa), C(c6d3ea), C(f2f5fa), C(0f1b33), C(e6ebf3), Pointer, false, None,     true,  C(0b140b), C(8ff06a), { C(f2f5fa), C(f2f5fa), C(f2f5fa), C(f2f5fa) } },
            { "fairlight",    C(d8d1bf), C(cbc3af), C(222222), C(3e3a33), C(3a7a4a), C(2a2724), C(3a7a4a), Membrane,false, Crt,      true,  C(0a1a0f), C(39ff6a), { C(3a7a4a), C(3a7a4a), C(3a7a4a), C(3a7a4a) } },
            { "virus",        C(b21f1f), C(c22626), C(fafafa), C(f0d0d0), C(fafafa), C(1a1a1a), C(fafafa), CapKnob, false, None,     true,  C(0e1a2a), C(7fd0ff), { C(fafafa), C(fafafa), C(fafafa), C(fafafa) } },
            { "nord",         C(c11d23), C(cf262c), C(fafafa), C(f4d6d6), C(fafafa), C(1c1c1c), C(fafafa), Pointer, false, None,     true,  C(1a0202), C(ff4a3a), { C(fafafa), C(fafafa), C(fafafa), C(fafafa) } },
            { "wavestation",  C(1b1b1d), C(252528), C(efefef), C(c4c4c8), C(c9a74a), C(101010), C(c9a74a), Membrane,false, None,     true,  C(8cb6e6), C(0a1422), { C(c9a74a), C(c9a74a), C(c9a74a), C(c9a74a) } },
            { "rompler",      C(222529), C(2c3035), C(f0f0f0), C(c0c5cc), C(7ac943), C(121416), C(7ac943), CapKnob, false, Rack,     true,  C(9ccf3c), C(13200a), { C(7ac943), C(7ac943), C(7ac943), C(7ac943) } },
            { "software",     C(14161a), C(1c1f25), C(eef1f6), C(9aa3b2), C(8a7bff), C(262a33), C(8a7bff), Modern,  false, None,     true,  C(0d0f14), C(8a7bff), { C(8a7bff), C(49e0b6), C(8a7bff), C(49e0b6) } },
            { "synclavier",   C(e7e3da), C(dcd7cc), C(1f1f1f), C(3a3a3a), C(d92f2f), C(2a2a2a), C(d92f2f), Membrane,false, None,     true,  C(1a0a05), C(ff7a3a), { C(d92f2f), C(d92f2f), C(d92f2f), C(d92f2f) } },
        };
        return s;
    }
    #undef C

    inline const Skin& find (const juce::String& key, bool digital)
    {
        for (auto& s : all()) if (key == s.key) return s;
        return all()[digital ? 1 : 0];
    }
}
