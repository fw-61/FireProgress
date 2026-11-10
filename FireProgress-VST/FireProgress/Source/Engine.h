// Fire Progress engine (no JUCE): traditions, ragas, styles, progressions, melody, humanize.
#pragma once
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <random>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace fire {

static const char* const NOTE[12]  = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};
static const char* const SWARA[12] = {"Sa","re","Re","ga","Ga","Ma","Ma^","Pa","dha","Dha","ni","Ni"};

// iv = custom intervals from the chord root (empty = use quality). label = custom name.
struct Chord { int offset = 0; int quality = 0; std::vector<int> iv; std::string label; bool swara = false; };
struct Note  { int step; int midi; int len; };                     // step = eighth-note index in the pattern
struct Ev    { double beat; bool on; int note; int vel; int ch; };
struct Seg   { Chord chord; int start; int len; };                 // one chord inside the pattern (beats)

inline std::vector<int> intervals(int q) {
    switch (q) {
        case 1:  return {0, 3, 7};
        case 2:  return {0, 4, 7, 10};
        case 3:  return {0, 4, 7, 11};
        case 4:  return {0, 3, 7, 10};
        default: return {0, 4, 7};
    }
}
inline std::vector<int> intervals(const Chord& c) { return c.iv.empty() ? intervals(c.quality) : c.iv; }
inline const char* suffix(int q) { static const char* s[] = {"", "m", "7", "maj7", "m7"}; return s[q]; }

inline std::string chordName(int root, const Chord& c) {
    if (!c.label.empty()) return c.label;
    return std::string(NOTE[(root + c.offset) % 12]) + suffix(c.quality);
}
inline std::string chordNotes(int root, const Chord& c) {
    std::string s;
    for (int iv : intervals(c)) {
        const int pc = (c.offset + iv) % 12;
        s += std::string(s.empty() ? "" : " ") + (c.swara ? SWARA[pc] : NOTE[(root + pc) % 12]);
    }
    return s;
}

// ================================================================= Western moods (vibes)
struct Vibe { std::string name; std::vector<std::string> major, minor; float density; int bpm; };

inline const std::vector<Vibe>& vibes() {
    static const std::vector<Vibe> v = {
        {"Happy",    {"I V vi IV", "I IV V I", "I vi IV V"},
                     {"i bIII bVII IV", "i bVII bVI bVII"}, 0.55f, 112},
        {"Sad",      {"vi IV I V", "I iii vi IV", "vi V IV V"},
                     {"i bVI bIII bVII", "i bVII bVI V", "i iv v i"}, 0.35f, 72},
        {"Chill",    {"ii7 V7 Imaj7 vi7", "Imaj7 iii7 vi7 ii7", "Imaj7 vi7 ii7 V7"},
                     {"i7 iv7 bVII7 bIIImaj7", "i7 bVImaj7 bIIImaj7 bVII"}, 0.30f, 80},
        {"Dreamy",   {"Imaj7 IVmaj7 iii7 vi7", "Imaj7 iii7 IVmaj7 V"},
                     {"i7 bIIImaj7 bVImaj7 bVII", "i7 iv7 bVImaj7 V7"}, 0.35f, 76},
        {"Dark",     {"I bVII bVI bVII", "I bVI bVII I"},
                     {"i bVI bVII i", "i iv bVI V", "i bII i V"}, 0.40f, 90},
        {"Epic",     {"IV V iii vi", "I iii IV V", "I V bVII IV"},
                     {"i bVI bIII bVII", "i bVII bVI bVII"}, 0.65f, 120},
        {"Romantic", {"I vi ii V", "I iii IV V", "I IV vi V"},
                     {"i bVI iv V", "i iv bVII bIII"}, 0.45f, 84},
    };
    return v;
}

// ================================================================= Styles (genres) 0-19 popular, 20-39 electronic, 40-54 other
// pat = chord rhythm, ext = chord colour (0 triads, 1 jazz 7ths, 2 power chords, 3 dominant 7ths),
// cs/ms = suggested sounds for chords/melody (0 piano, 1 synth, 2 pad)
struct Style { const char* name; int cat; int bpm; float dens; float swing; int pat; int ext; int cs; int ms; };

inline const std::vector<Style>& styles() {
    static const std::vector<Style> v = {
        {"Pop",0,110,.55f,0.f,10,0,0,1}, {"Hip-Hop",0,90,.40f,.15f,9,1,0,1}, {"Rap",0,95,.35f,.10f,9,0,2,0},
        {"R&B",0,85,.45f,.20f,8,1,0,1}, {"Rock",0,120,.50f,0.f,7,2,1,1}, {"Metal",0,150,.65f,0.f,7,2,1,1},
        {"Jazz",0,130,.55f,.30f,8,1,0,0}, {"Blues",0,100,.45f,.30f,8,3,0,1}, {"Soul",0,90,.45f,.10f,8,1,0,1},
        {"Funk",0,105,.60f,0.f,6,3,1,1}, {"Reggae",0,75,.40f,0.f,2,0,0,1}, {"Country",0,100,.45f,0.f,5,0,0,1},
        {"Folk",0,95,.40f,0.f,4,0,0,0}, {"Gospel",0,80,.45f,.10f,1,1,2,0}, {"Disco",0,118,.55f,0.f,6,1,1,1},
        {"Punk",0,175,.60f,0.f,7,2,1,1}, {"Indie",0,110,.45f,0.f,10,0,0,1}, {"Alternative",0,115,.50f,0.f,10,0,1,1},
        {"Grunge",0,120,.45f,0.f,7,2,1,1}, {"Ska",0,160,.55f,0.f,2,0,0,1},

        {"EDM",1,128,.60f,0.f,3,0,1,1}, {"Techno",1,130,.35f,0.f,3,0,1,1}, {"House",1,124,.45f,0.f,2,1,0,1},
        {"Deep House",1,122,.40f,0.f,2,1,0,1}, {"Progressive House",1,126,.50f,0.f,4,0,1,1},
        {"Trance",1,138,.55f,0.f,4,0,1,1}, {"Psytrance",1,145,.60f,0.f,4,0,1,1}, {"Drum & Bass",1,174,.55f,0.f,0,1,2,1},
        {"Dubstep",1,140,.40f,0.f,9,0,1,1}, {"Future Bass",1,150,.60f,0.f,0,1,1,1}, {"Hardstyle",1,150,.55f,0.f,3,0,1,1},
        {"Hardcore",1,180,.60f,0.f,3,0,1,1}, {"Ambient",1,70,.20f,0.f,0,1,2,0}, {"Downtempo",1,90,.30f,.10f,9,1,2,0},
        {"Electro",1,125,.50f,0.f,3,0,1,1}, {"Synthwave",1,100,.50f,0.f,4,0,1,1}, {"Industrial",1,130,.40f,0.f,7,2,1,1},
        {"Minimal Techno",1,126,.25f,0.f,3,0,1,1}, {"Melodic Techno",1,124,.45f,0.f,4,0,1,1}, {"Breakbeat",1,135,.50f,.10f,6,0,1,1},

        {"Orchestral",2,90,.50f,0.f,0,0,2,0}, {"Cinematic",2,80,.35f,0.f,4,0,2,0}, {"Soundtrack",2,85,.40f,0.f,0,1,2,0},
        {"Opera",2,75,.45f,0.f,0,0,2,0}, {"Musical Theatre",2,105,.50f,.05f,10,0,0,0}, {"Choral",2,70,.30f,0.f,0,0,2,2},
        {"Acapella",2,90,.45f,0.f,0,0,2,2}, {"Experimental",2,100,.50f,0.f,0,1,1,1}, {"New Age",2,65,.25f,0.f,4,0,2,0},
        {"Meditation Music",2,55,.15f,0.f,0,0,2,2}, {"Spiritual Music",2,70,.30f,0.f,0,0,2,0},
        {"World Fusion",2,100,.50f,0.f,11,0,0,1}, {"Tribal Music",2,100,.50f,0.f,1,0,1,1}, {"Lo-fi",2,80,.35f,.20f,8,1,0,0},
        {"New Wave",2,125,.50f,0.f,1,0,1,1},
    };
    return v;
}
inline const char* patName(int p) {
    static const char* n[] = {"Sustained", "Pulse", "Off-beat stabs", "Four-on-floor", "Arpeggio", "Boom-chick",
                              "Funk stabs", "Power chugs", "Swing comping", "Hip-hop", "Strum", "Tresillo"};
    return n[std::clamp(p, 0, 11)];
}
inline const char* soundName(int s) { static const char* n[] = {"Piano", "Synth", "Pad"}; return n[std::clamp(s, 0, 2)]; }

// ================================================================= World traditions (1-20, same order as the list)
// functional (modal=false): Roman-numeral progressions, scale from Major/Minor. modal=true: scale + degree progressions.
struct Trad { std::string name; bool modal; std::vector<int> scale, minScale; std::vector<std::string> major, minor; int bpm; };

inline const std::vector<Trad>& trads() {
    static const std::vector<int> MAJ = {0,2,4,5,7,9,11}, MIN = {0,2,3,5,7,8,10};
    static const std::vector<Trad> v = {
        {"Indian Music",        true,  {0,2,4,5,7,9,10}, {}, {"0 3 4 3","0 6 3 4","0 4 5 3"}, {}, 90},
        {"Western Music",       false, MAJ, MIN, {}, {}, 100},
        {"African Music",       true,  {0,2,4,7,9}, {}, {"0 3 4 0","0 2 3 4","0 1 3 4"}, {}, 105},
        {"Arabic Music",        true,  {0,1,4,5,7,8,10}, {}, {"0 1 0 4","0 3 1 0","0 4 3 1"}, {}, 95},
        {"Chinese Music",       true,  {0,2,5,7,9}, {}, {"0 2 3 1","0 3 2 4","0 1 3 4"}, {}, 85},
        {"Japanese Music",      true,  {0,2,3,7,8}, {}, {"0 2 3 1","0 3 1 4","0 1 2 4"}, {}, 80},
        {"Korean Music",        true,  {0,3,5,7,10}, {}, {"0 2 3 4","0 3 1 4","0 1 2 3"}, {}, 90},
        {"Latin American Music",false, MAJ, MIN, {"I IV V IV","I vi ii V","I V IV V"}, {"i iv V i","i bVII bVI V","i bVI bVII i"}, 105},
        {"Brazilian Music",     false, MAJ, MIN, {"ii7 V7 Imaj7 vi7","Imaj7 vi7 ii7 V7","Imaj7 IVmaj7 iii7 vi7"},
                                                 {"i7 iv7 V7 i7","i7 bVImaj7 ii7 V7","i7 bVII7 bVImaj7 V7"}, 95},
        {"Spanish Music",       false, MAJ, {0,1,4,5,7,8,10}, {"I IV V IV","I ii V I","I V IV V"}, {"i bVII bVI V","i bII i V","i iv V i"}, 115},
        {"Celtic Music",        false, {0,2,4,5,7,9,10}, {0,2,3,5,7,9,10}, {"I IV I V","I bVII IV I","I V vi IV"},
                                                 {"i bVII bVI bVII","i bIII bVII i","i IV bVII i"}, 110},
        {"Persian Music",       true,  {0,1,4,5,7,8,11}, {}, {"0 1 0 4","0 4 5 0","0 3 1 4"}, {}, 90},
        {"Turkish Music",       true,  {0,1,3,5,7,8,10}, {}, {"0 6 5 0","0 3 6 0","0 1 0 6"}, {}, 100},
        {"Greek Music",         true,  {0,2,3,5,7,8,11}, {}, {"0 3 4 0","0 5 4 0","0 3 6 0"}, {}, 105},
        {"Sri Lankan Music",    false, MAJ, MIN, {"I vi IV V","I IV V I","I V I IV"}, {"i iv V i","i bVII bVI V","i bVI iv V"}, 105},
        {"Indonesian Music",    true,  {0,1,3,7,8}, {}, {"0 2 4 1","0 3 2 4","0 1 3 2"}, {}, 95},
        {"Thai Music",          true,  {0,2,3,5,7,9,10}, {}, {"0 3 4 0","0 1 4 3","0 5 3 4"}, {}, 100},
        {"Tibetan Music",       true,  {0,2,5,7,10}, {}, {"0 3 0 3","0 2 0 4","0 4 3 0"}, {}, 70},
        {"Native American Music",true, {0,3,5,8,10}, {}, {"0 2 0 4","0 3 2 0","0 4 2 0"}, {}, 85},
        {"Caribbean Music",     false, MAJ, MIN, {"I IV I V","I V IV I","I vi IV V"}, {"i iv V i","i bVII i V","i bVI bVII i"}, 100},
    };
    return v;
}

// ================================================================= Indian ragas
// notes = every pitch of the raga (semitones from Sa). ascOnly/descOnly = notes used only going up / only going down.
// chordScale = notes used for chords (empty = notes without the ascOnly/descOnly ones). pakad = signature phrase.
struct Raga { std::string name, mood; std::vector<int> notes, chordScale, ascOnly, descOnly; int vadi, samvadi; std::vector<int> pakad; int bpm; float dens; };

inline const std::vector<Raga>& ragas() {
    static const std::vector<Raga> v = {
        {"Yaman",          "Peaceful, Romantic",     {0,2,4,6,7,9,11}, {}, {}, {}, 4, 11, {-1,2,4,6,7,4,2,0}, 80, .40f},
        {"Bhairav",        "Devotional, Serious",    {0,1,4,5,7,8,11}, {}, {}, {}, 8, 1,  {0,4,5,7,8,7,5,4,1,0}, 70, .35f},
        {"Bhairavi",       "Emotional, Devotional",  {0,1,3,5,7,8,10}, {}, {}, {}, 5, 0,  {0,1,3,5,7,5,3,1,0}, 72, .40f},
        {"Darbari Kanada", "Deep, Dark, Royal",      {0,2,3,5,7,8,10}, {}, {}, {}, 2, 7,  {-2,0,2,3,5,3,2,0}, 60, .30f},
        {"Malkauns",       "Mysterious, Meditative", {0,3,5,8,10},     {}, {}, {}, 5, 0,  {-2,0,3,5,8,5,3,0}, 62, .30f},
        {"Marwa",          "Tension, Mystery",       {0,1,4,6,9,11},   {}, {}, {}, 9, 1,  {-3,-1,1,4,6,4,1,-1}, 68, .35f},
        {"Puriya",         "Deep, Serious",          {0,1,4,6,9,11},   {}, {}, {}, 4, 11, {-1,1,4,6,4,6,9,11}, 66, .35f},
        {"Bageshri",       "Romantic, Emotional",    {0,2,3,5,7,9,10}, {}, {}, {7}, 5, 0, {-3,-2,0,5,3,2,0}, 76, .40f},
        {"Bhimpalasi",     "Sad, Longing",           {0,2,3,5,7,9,10}, {}, {}, {2,9}, 5, 0, {-2,0,3,5,7,5,3,2,0}, 74, .40f},
        {"Desh",           "Patriotic, Joyful",      {0,2,4,5,7,9,10,11}, {0,2,4,5,7,9}, {11}, {10}, 2, 7, {2,5,7,11,9,7,5,4,2,0}, 96, .50f},
        {"Kafi",           "Romantic, Folk",         {0,2,3,5,7,9,10}, {}, {}, {}, 7, 0,  {0,2,3,5,7,5,3,2,0}, 88, .45f},
        {"Khamaj",         "Romantic, Playful",      {0,2,4,5,7,9,10,11}, {0,4,5,7,9}, {11}, {10,2}, 4, 11, {4,5,7,9,11,9,7,5,4,2,0}, 100, .50f},
        {"Todi",           "Sad, Intense",           {0,1,3,6,7,8,11}, {}, {}, {}, 8, 3,  {0,1,3,6,8,7,6,3,1,0}, 70, .40f},
        {"Ahir Bhairav",   "Peaceful, Spiritual",    {0,1,4,5,7,9,10}, {}, {}, {}, 5, 0,  {0,1,4,5,9,7,5,4,1,0}, 74, .40f},
        {"Jog",            "Mysterious, Romantic",   {0,3,4,5,7,10},   {0,4,5,7,10}, {}, {}, 4, 10, {0,3,4,5,7,10,7,5,3,0}, 72, .40f},
        {"Jaunpuri",       "Sad, Emotional",         {0,2,3,5,7,8,10}, {}, {}, {}, 8, 3,  {0,2,5,7,8,7,5,3,2,0}, 70, .40f},
        {"Chandrakauns",   "Dark, Mysterious",       {0,3,5,8,11},     {}, {}, {}, 5, 0,  {0,3,5,8,11,8,5,3,0}, 64, .30f},
        {"Shree",          "Serious, Majestic",      {0,1,4,6,7,8,11}, {}, {}, {4,8}, 1, 7, {0,1,6,7,11,8,7,6,4,1,0}, 66, .35f},
        {"Hamsadhwani",    "Happy, Energetic",       {0,2,4,7,11},     {}, {}, {}, 4, 11, {0,2,4,7,11,7,4,2,0}, 112, .55f},
        {"Megh",           "Rainy, Peaceful",        {0,2,5,7,10},     {}, {}, {}, 0, 7,  {0,2,5,2,0,-2,0}, 76, .40f},
    };
    return v;
}

// ================================================================= Setup: everything the generator needs
// trad: 0 = not chosen yet (plays Western), 1..20. raga: -1 = off, 0..19. sg/se/so: global style index or -1.
struct Setup {
    int root = 0; bool minor = false; int trad = 0; int vibe = 0; int raga = -1;
    int sg = -1, se = -1, so = -1;
    int source = 0, prog = 0, gen = 1, count = 4, bars = 4;
};

inline int tradEff(const Setup& s) { return s.trad <= 0 ? 2 : std::clamp(s.trad, 1, 20); }
inline const Trad& tradOf(const Setup& s) { return trads()[(size_t) tradEff(s) - 1]; }
inline bool isRaga(const Setup& s) { return s.raga >= 0 && s.raga < 20; }
inline bool isModal(const Setup& s) { return isRaga(s) || tradOf(s).modal; }
inline bool hasStyle(const Setup& s) { return s.sg >= 0 || s.se >= 0 || s.so >= 0; }

struct Mix { bool valid = false; int bpm = 100; float dens = 0.5f; float swing = 0.f; int pat = 0; int ext = 0; int cs = 0; int ms = 1; };

// Blend of the chosen styles (or the raga). Without any style the plugin behaves exactly like before.
inline Mix mixOf(const Setup& s) {
    Mix m;
    if (isRaga(s)) {
        const Raga& r = ragas()[(size_t) s.raga];
        m.valid = true; m.bpm = r.bpm; m.dens = r.dens; m.cs = 2; m.ms = 1;
        return m;
    }
    m.bpm = tradOf(s).bpm;
    m.dens = vibes()[(size_t) std::clamp(s.vibe, 0, 6)].density;
    const int idx[3] = {s.sg, s.se, s.so};
    int n = 0; double bpm = 0, dens = 0, sw = 0; bool first = true;
    for (int k : idx) {
        if (k < 0 || k >= (int) styles().size()) continue;
        const Style& st = styles()[(size_t) k];
        ++n; bpm += st.bpm; dens += st.dens; sw += st.swing;
        if (first) { m.pat = st.pat; m.ext = st.ext; m.cs = st.cs; m.ms = st.ms; first = false; }
        else if (m.ext == 0) m.ext = st.ext;
    }
    if (n > 0) { m.valid = true; m.bpm = (int) std::lround(bpm / n); m.dens = (float) (dens / n); m.swing = (float) (sw / n); }
    return m;
}

inline bool has(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }

// scale used for the melody (semitones from the root / Sa)
inline std::vector<int> melodyScale(const Setup& s) {
    if (isRaga(s)) return ragas()[(size_t) s.raga].notes;
    const Trad& t = tradOf(s);
    if (t.modal) return t.scale;
    return (s.minor && !t.minScale.empty()) ? t.minScale : t.scale;
}
// scale used to build modal chords
inline std::vector<int> modalScale(const Setup& s) {
    if (isRaga(s)) {
        const Raga& r = ragas()[(size_t) s.raga];
        if (!r.chordScale.empty()) return r.chordScale;
        std::vector<int> v;
        for (int x : r.notes) if (!has(r.ascOnly, x) && !has(r.descOnly, x)) v.push_back(x);
        return v;
    }
    return tradOf(s).scale;
}

// ================================================================= Chords from tokens
inline Chord parseChord(const std::string& t) {
    size_t i = 0; int shift = 0;
    if (t[i] == 'b') { shift = -1; ++i; } else if (t[i] == '#') { shift = 1; ++i; }
    const std::string rest = t.substr(i);
    static const char* nums[] = {"VII", "VI", "IV", "V", "III", "II", "I"};
    static const int offs[]   = {11, 9, 5, 7, 4, 2, 0};
    for (int k = 0; k < 7; ++k) {
        std::string U = nums[k], L = U;
        for (auto& c : L) c = (char) std::tolower((unsigned char) c);
        const bool up  = rest.compare(0, U.size(), U) == 0;
        const bool low = rest.compare(0, L.size(), L) == 0;
        if (up || low) {
            const std::string suf = rest.substr(U.size());
            Chord c;
            c.offset = (offs[k] + shift + 12) % 12;
            c.quality = (suf == "maj7") ? 3 : (suf == "7") ? (up ? 2 : 4) : (up ? 0 : 1);
            return c;
        }
    }
    return Chord();
}
inline std::vector<Chord> parseProgression(const std::string& s) {
    std::istringstream ss(s);
    std::string tok; std::vector<Chord> r;
    while (ss >> tok) r.push_back(parseChord(tok));
    return r;
}

// style colour on a Roman-numeral chord
inline Chord styleChord(Chord c, int ext, int root) {
    if (ext == 1)      { if (c.quality == 0) c.quality = (c.offset == 7 || c.offset == 10) ? 2 : 3; else if (c.quality == 1) c.quality = 4; }
    else if (ext == 3) { if (c.quality == 0 || c.quality == 3) c.quality = 2; else if (c.quality == 1) c.quality = 4; }
    else if (ext == 2) { c.iv = {0, 7, 12}; c.label = std::string(NOTE[(root + c.offset) % 12]) + "5"; }
    return c;
}

// chord built only from notes of the scale (stacked thirds inside the scale)
inline Chord modalChord(const std::vector<int>& sc, int deg, int ext, const Setup& s) {
    const int n = (int) sc.size();
    auto at = [&](int k) { return sc[(size_t) (k % n)] + 12 * (k / n); };
    Chord c;
    c.offset = sc[(size_t) deg];
    if (ext == 2) c.iv = {0, at(deg + 4) - at(deg), 12};
    else {
        c.iv = {0, at(deg + 2) - at(deg), at(deg + 4) - at(deg)};
        if (ext == 1 || ext == 3) c.iv.push_back(at(deg + 6) - at(deg));
    }
    if (isRaga(s)) { c.label = SWARA[c.offset % 12]; c.swara = true; }
    else c.label = NOTE[(s.root + c.offset) % 12];
    return c;
}

inline std::vector<Chord> chordsFromTokens(const Setup& s, const std::string& tok) {
    const int ext = isRaga(s) ? 0 : mixOf(s).ext;
    std::vector<Chord> out;
    if (isModal(s)) {
        const auto sc = modalScale(s);
        std::istringstream ss(tok); int d;
        while (ss >> d) out.push_back(modalChord(sc, ((d % (int) sc.size()) + (int) sc.size()) % (int) sc.size(), ext, s));
    } else {
        for (auto& c : parseProgression(tok)) out.push_back(styleChord(c, ext, s.root));
    }
    return out;
}
inline std::string progNames(const Setup& s, const std::string& tok) {
    std::string out;
    for (auto& c : chordsFromTokens(s, tok)) out += std::string(out.empty() ? "" : " ") + chordName(s.root, c);
    return out;
}

// ================================================================= Progressions
inline const std::vector<std::string>& progList(int vibe, bool minor) {
    const auto& v = vibes()[(size_t) vibe];
    return minor ? v.minor : v.major;
}
inline const std::vector<std::string>& popularList(const Setup& s) {
    static const std::vector<std::string> none;
    if (isRaga(s)) return none;
    const Trad& t = tradOf(s);
    if (t.modal) return t.major;
    if (tradEff(s) == 2) return progList(std::clamp(s.vibe, 0, 6), s.minor);
    return s.minor ? t.minor : t.major;
}

inline std::string sevenify(const std::string& t) {
    const size_t i = (t[0] == 'b' || t[0] == '#') ? 1 : 0;
    if (t == "V" || t == "bVII") return t + "7";
    return std::isupper((unsigned char) t[i]) ? t + "maj7" : t + "7";
}

// Roman-numeral walk that starts on the main chord (functional systems)
inline std::string generatedRoman(int vibe, bool minor, unsigned seed, int count) {
    count = std::clamp(count, 1, 8);
    using Row = std::vector<std::pair<const char*, int>>;
    static const std::map<std::string, Row> MAJ = {
        {"I",   {{"IV",3},{"V",3},{"vi",3},{"ii",2},{"iii",1}}},
        {"ii",  {{"V",4},{"IV",1},{"vi",1}}},
        {"iii", {{"vi",3},{"IV",3},{"ii",1}}},
        {"IV",  {{"V",3},{"I",2},{"vi",1},{"ii",1}}},
        {"V",   {{"I",3},{"vi",3},{"IV",1}}},
        {"vi",  {{"IV",3},{"ii",2},{"V",2},{"iii",1}}}};
    static const std::map<std::string, Row> MIN = {
        {"i",    {{"bVI",3},{"bVII",3},{"iv",2},{"bIII",2},{"V",1}}},
        {"bIII", {{"bVII",3},{"iv",2},{"bVI",2},{"i",1}}},
        {"iv",   {{"V",2},{"bVII",2},{"i",2},{"bVI",1}}},
        {"V",    {{"i",4},{"bVI",2}}},
        {"bVI",  {{"bVII",3},{"bIII",2},{"iv",1},{"V",1},{"i",1}}},
        {"bVII", {{"i",3},{"bIII",2},{"bVI",2}}}};
    const auto& T = minor ? MIN : MAJ;
    std::mt19937 rng(seed * 2246822519u + 3266489917u);
    const std::string tonic = minor ? "i" : "I";
    std::vector<std::string> t;
    for (int attempt = 0; attempt < 20; ++attempt) {
        t.assign(1, tonic);
        for (int i = 1; i < count; ++i) {
            const Row& row = T.at(t.back());
            int sum = 0; for (auto& e : row) sum += e.second;
            int r = (int) (rng() % (unsigned) sum);
            for (auto& e : row) { if (r < e.second) { t.push_back(e.first); break; } r -= e.second; }
        }
        if (count <= 1 || t.back() != tonic) break;
    }
    const std::string name = vibes()[(size_t) std::clamp(vibe, 0, 6)].name;
    for (auto& s : t) {
        if (name == "Chill" || name == "Dreamy") s = sevenify(s);
        else if (!minor && name == "Dark")  { if (s == "V" && rng() % 10 < 7) s = "bVII"; else if (s == "IV" && rng() % 10 < 7) s = "iv"; }
        else if (!minor && name == "Epic")  { if (s == "V" && rng() % 10 < 4) s = "bVII"; }
    }
    std::string out;
    for (auto& s : t) out += (out.empty() ? "" : " ") + s;
    return out;
}

// Degree walk inside a scale (modal systems, ragas). Ragas favour their vadi / samvadi.
inline std::string generatedModal(const Setup& s, unsigned seed, int count) {
    count = std::clamp(count, 1, 8);
    const auto sc = modalScale(s);
    const int n = (int) sc.size();
    std::vector<int> w((size_t) n, 1);
    auto nearest = [&](int target) { int b = 0; for (int i = 0; i < n; ++i) if (std::abs(sc[(size_t) i] - target) < std::abs(sc[(size_t) b] - target)) b = i; return b; };
    w[0] += 2;
    if (isRaga(s)) { const Raga& r = ragas()[(size_t) s.raga]; w[(size_t) nearest(r.vadi)] += 3; w[(size_t) nearest(r.samvadi)] += 2; }
    else { w[(size_t) nearest(7)] += 2; w[(size_t) nearest(5)] += 1; }
    std::mt19937 rng(seed * 2246822519u + 3266489917u);
    std::vector<int> t;
    for (int attempt = 0; attempt < 20; ++attempt) {
        t.assign(1, 0);
        for (int i = 1; i < count; ++i) {
            int sum = 0; for (int d = 0; d < n; ++d) if (d != t.back()) sum += w[(size_t) d];
            int r = (int) (rng() % (unsigned) sum);
            for (int d = 0; d < n; ++d) { if (d == t.back()) continue; if (r < w[(size_t) d]) { t.push_back(d); break; } r -= w[(size_t) d]; }
        }
        if (count <= 1 || t.back() != 0) break;
    }
    std::string out;
    for (int d : t) out += (out.empty() ? "" : " ") + std::to_string(d);
    return out;
}

inline std::string generatedTokens(const Setup& s) {
    return isModal(s) ? generatedModal(s, (unsigned) s.gen, s.count) : generatedRoman(s.vibe, s.minor, (unsigned) s.gen, s.count);
}
inline std::string popularTokens(const Setup& s, int idx) {
    const auto& l = popularList(s);
    if (l.empty()) return generatedTokens(s);
    std::istringstream ss(l[(size_t) idx % l.size()]);
    std::vector<std::string> base; std::string t;
    while (ss >> t) base.push_back(t);
    std::string out;
    for (int i = 0; i < std::clamp(s.count, 1, 8); ++i) out += (out.empty() ? "" : " ") + base[(size_t) i % base.size()];
    return out;
}
inline std::vector<Chord> resolveChords(const Setup& s) {
    const std::string tok = (isRaga(s) || s.source == 1) ? generatedTokens(s) : popularTokens(s, s.prog);
    return chordsFromTokens(s, tok);
}

// Spread the chords over the pattern (4 or 8 bars). Chord changes land on 2-beat slots.
inline std::vector<Seg> layout(const std::vector<Chord>& ch, int bars) {
    const int n = (int) ch.size(), slots = bars * 2;
    std::vector<Seg> v;
    for (int i = 0; i < n; ++i) {
        const int a = (int) std::lround((double) i * slots / n) * 2;
        const int b = (i + 1 < n) ? (int) std::lround((double) (i + 1) * slots / n) * 2 : bars * 4;
        v.push_back({ch[(size_t) i], a, b - a});
    }
    return v;
}
inline std::vector<Seg> buildSegs(const Setup& s) { return layout(resolveChords(s), s.bars); }

// ================================================================= Melody
// Deterministic: same setup + same seed = same melody. Raga mode: only raga notes, ascent/descent rules,
// vadi/samvadi emphasis, the raga's signature phrase, and a landing on Sa.
inline std::vector<Note> makeMelody(const Setup& s, const std::vector<Seg>& segs, unsigned seed) {
    std::mt19937 rng(seed * 2654435761u + 12345u);
    auto rnd = [&]() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); };
    const int root = s.root, steps = s.bars * 8;
    const Raga* R = isRaga(s) ? &ragas()[(size_t) s.raga] : nullptr;
    const auto sc = melodyScale(s);
    bool inScale[12] = {false};
    for (int x : sc) inScale[(x + root) % 12] = true;
    std::vector<int> L;
    for (int m = 60; m <= 84; ++m) if (inScale[m % 12]) L.push_back(m);
    auto pcOf = [&](int m) { return ((m - root) % 12 + 12) % 12; };
    auto allowed = [&](int m, bool up) {
        if (!R) return true;
        const int pc = pcOf(m);
        if (up && has(R->descOnly, pc)) return false;
        if (!up && has(R->ascOnly, pc)) return false;
        return true;
    };
    const float dens = mixOf(s).dens;

    int sa = 60 + root; if (sa < 63) sa += 12;
    std::vector<int> forced((size_t) steps, -1);
    if (R)
        for (int st = 0; st + (int) R->pakad.size() <= steps; st += 16) {
            if (st > 0 && rnd() < 0.25f) continue;
            for (size_t k = 0; k < R->pakad.size(); ++k) forced[(size_t) st + k] = sa + R->pakad[k];
        }

    int prev = L[L.size() / 2];
    std::vector<Note> out;
    for (int st = 0; st < steps; ++st) {
        if (forced[(size_t) st] >= 0) { out.push_back({st, forced[(size_t) st], 1}); prev = forced[(size_t) st]; continue; }
        size_t k = 0;
        for (size_t j = 0; j < segs.size(); ++j) if (st / 2 >= segs[j].start) k = j;
        const Seg& sg = segs[k];
        const bool strong = (st % 4 == 0) || (st == sg.start * 2);
        if (!strong && rnd() > dens) continue;
        int n;
        if (strong || rnd() < 0.25f) {
            if (R && rnd() < 0.55f) {
                std::vector<int> E;
                for (int m = 60; m <= 84; ++m) { const int pc = pcOf(m); if (pc == R->vadi || pc == R->samvadi || pc == 0) E.push_back(m); }
                std::sort(E.begin(), E.end(), [&](int x, int y) { return std::abs(x - prev) < std::abs(y - prev); });
                n = E[(rnd() < 0.7f || E.size() < 2) ? 0 : 1];
            } else {
                bool inChord[12] = {false};
                for (int iv : intervals(sg.chord)) inChord[(root + sg.chord.offset + iv) % 12] = true;
                std::vector<int> C;
                for (int m = 60; m <= 84; ++m) if (inChord[m % 12]) C.push_back(m);
                std::sort(C.begin(), C.end(), [&](int x, int y) { return std::abs(x - prev) < std::abs(y - prev); });
                n = C[(rnd() < 0.7f || C.size() < 2) ? 0 : 1];
            }
        } else {
            int i = 0;
            for (size_t j = 0; j < L.size(); ++j) if (std::abs(L[j] - prev) < std::abs(L[(size_t) i] - prev)) i = (int) j;
            static const int stp[6] = {-2, -1, -1, 1, 1, 2};
            const int d = stp[(int) (rnd() * 6) % 6];
            const bool up = d > 0;
            int j = std::clamp(i + d, 0, (int) L.size() - 1);
            while (j >= 0 && j < (int) L.size() && !allowed(L[(size_t) j], up)) j += up ? 1 : -1;
            if (j < 0 || j >= (int) L.size()) j = i;
            n = L[(size_t) j];
        }
        out.push_back({st, n, 1});
        prev = n;
    }
    if (R && !out.empty()) {                                  // end on Sa
        int best = out.back().midi;
        for (int m = 60; m <= 84; ++m) if (pcOf(m) == 0 && std::abs(m - out.back().midi) < std::abs(best - out.back().midi)) best = m;
        if (pcOf(out.back().midi) != 0) out.back().midi = best;
    }
    const int cap = R ? 6 : 4;
    for (size_t i = 0; i < out.size(); ++i) {
        const int nx = (i + 1 < out.size()) ? out[i + 1].step : steps;
        out[i].len = std::min(nx - out[i].step, cap);
    }
    return out;
}

// Fingerprint of a melody (notes + rhythm) used to remember which melodies were already used.
inline uint64_t melodyHash(const std::vector<Note>& m) {
    uint64_t h = 1469598103934665603ull;
    for (auto& n : m)
        for (int v : {n.step, n.midi, n.len}) { h ^= (uint64_t) (v + 1000); h *= 1099511628211ull; }
    return h;
}

// ================================================================= MIDI events
// Chord rhythms. Hit: t = eighth inside the bar (0-7), len in eighths, what: 0 chord, 1 bass root, 9 bass fifth,
// 3 power chord, 20 arpeggio note. Pattern 0 = sustained chord.
struct Hit { int t, len, what; };
inline const std::vector<std::vector<Hit>>& chordPatterns() {
    static const std::vector<std::vector<Hit>> P = {
        {},
        {{0,2,0},{2,2,0},{4,2,0},{6,2,0},{0,2,1}},
        {{0,4,1},{4,4,1},{1,1,0},{3,1,0},{5,1,0},{7,1,0}},
        {{0,1,1},{2,1,1},{4,1,1},{6,1,1},{1,1,0},{3,1,0},{5,1,0},{7,1,0}},
        {{0,1,20},{1,1,20},{2,1,20},{3,1,20},{4,1,20},{5,1,20},{6,1,20},{7,1,20}},
        {{0,2,1},{2,2,0},{4,2,9},{6,2,0}},
        {{0,2,1},{3,1,0},{5,1,0},{7,1,0}},
        {{0,1,3},{1,1,3},{2,1,3},{3,1,3},{4,1,3},{5,1,3},{6,1,3},{7,1,3}},
        {{0,3,0},{5,2,0},{0,3,1}},
        {{0,8,0},{0,3,1},{3,1,1},{6,1,1}},
        {{0,2,0},{2,1,0},{3,1,0},{5,1,0},{6,2,0},{0,2,1}},
        {{0,3,0},{3,3,0},{6,2,0},{0,2,1},{4,2,1}}};
    return P;
}

// human = 0..1: loose timing, velocity accents, varied note lengths, light chord strum. Style adds chord rhythm + swing.
// Deterministic for a given seed, so playback and exported MIDI match.
inline std::vector<Ev> buildEvents(const Setup& s, const std::vector<Seg>& segs, const std::vector<Note>& mel,
                                   bool chords, bool melody, float human, unsigned seed) {
    const Mix mx = mixOf(s);
    const int root = s.root;
    std::mt19937 rng(seed * 747796405u + 2891336453u);
    auto rnd = [&]() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); };
    auto jit = [&](double a) { return (rnd() - 0.5) * 2.0 * a; };
    const double L = s.bars * 4.0, h = std::clamp((double) human, 0.0, 1.0), swing = mx.swing * 0.5;
    auto vel = [&](double base, double spread) { return std::clamp((int) std::lround(base + jit(spread * h)), 1, 127); };
    std::vector<Ev> ev;

    if (chords) {
        const auto& hits = chordPatterns()[(size_t) std::clamp(mx.pat, 0, 11)];
        for (auto& sg : segs) {
            const int base = 48 + (root + sg.chord.offset) % 12;
            const auto iv = intervals(sg.chord);
            const double t0 = sg.start, tEnd = sg.start + sg.len;
            auto add = [&](int note, int v, double on0, double off0, double delay) {
                const double on = std::max(0.0, on0 + delay + jit(0.012 * h));
                double off = std::min(L - 0.01, off0 + jit(0.05 * h));
                if (off < on + 0.1) off = on + 0.1;
                ev.push_back({on, true, note, v, 1});
                ev.push_back({off, false, note, 0, 1});
            };
            if (hits.empty()) {
                add(base - 12, vel(85, 10), t0, tEnd - 0.05, 0.0);
                int k = 0;
                for (int x : iv) add(base + x, vel(70, 12), t0, tEnd - 0.05, 0.03 * (++k) * h);
            } else {
                for (int bar = sg.start / 4; bar * 4 < sg.start + sg.len; ++bar)
                    for (auto& hit : hits) {
                        const double t = bar * 4.0 + hit.t * 0.5;
                        if (t < t0 - 1e-9 || t >= tEnd - 1e-9) continue;
                        const double len = std::min(hit.len * 0.5, tEnd - t);
                        const double sw = (hit.t % 2 == 1) ? swing : 0.0;
                        const double on0 = t + sw, off0 = t + sw + len * 0.9;
                        std::vector<int> ns;
                        if (hit.what == 1)       ns = {base - 12};
                        else if (hit.what == 9)  ns = {base - 12 + 7};
                        else if (hit.what == 3)  ns = {base, base + 7, base + 12};
                        else if (hit.what == 20) { auto seq = iv; seq.push_back(iv[0] + 12); ns = {base + seq[(size_t) hit.t % seq.size()]}; }
                        else for (int x : iv) ns.push_back(base + x);
                        const int v = vel((hit.what == 1 ? 82 : 70) + (hit.t == 0 ? 8 : 0), 12);
                        int k = 0;
                        for (int nn : ns) add(nn, v, on0, off0, hit.what == 0 ? 0.02 * (k++) * h : 0.0);
                    }
            }
        }
    }

    if (melody) {
        std::vector<double> on(mel.size());
        for (size_t i = 0; i < mel.size(); ++i)
            on[i] = std::max(0.0, mel[i].step * 0.5 + (mel[i].step % 2 ? swing : 0.0) + jit(0.045 * h));
        for (size_t i = 0; i < mel.size(); ++i) {
            const bool strong = mel[i].step % 4 == 0;
            const int v = vel(98.0 + (strong ? 9.0 * h : 0.0), 14.0);
            const double nextOn = (i + 1 < mel.size()) ? on[i + 1] : L;
            double off = on[i] + mel[i].len * 0.5 * (1.0 - 0.22 * h * rnd()) - 0.03;
            off = std::min({off, nextOn - 0.02, L - 0.01});
            if (off < on[i] + 0.05) off = on[i] + 0.05;
            ev.push_back({on[i], true, mel[i].midi, v, 2});
            ev.push_back({off, false, mel[i].midi, 0, 2});
        }
    }
    return ev;
}

// Raw parameter values (choice indexes) -> Setup. Used by the processor, the editor and the history.
inline Setup setupFromParams(int root, int mode, int vibe, int prog, int source, int gen, int barsIdx, int countIdx,
                             int trad, int ragaIdx, int sgenIdx, int selecIdx, int sotherIdx) {
    Setup s;
    s.root = root; s.minor = (mode == 1); s.vibe = vibe; s.prog = prog; s.source = source; s.gen = gen;
    s.bars = (barsIdx == 1) ? 8 : 4; s.count = countIdx + 1;
    s.trad = trad; s.raga = ragaIdx - 1;
    s.sg = sgenIdx - 1;
    s.se = selecIdx == 0 ? -1 : 19 + selecIdx;
    s.so = sotherIdx == 0 ? -1 : 39 + sotherIdx;
    return s;
}

} // namespace fire
