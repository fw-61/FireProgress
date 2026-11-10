// Fire Progress engine: progression library, progression generator, melody generator (no JUCE dependency).
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

static const char* const NOTE[12] = {"C","C#","D","Eb","E","F","F#","G","Ab","A","Bb","B"};

struct Chord { int offset; int quality; };      // quality: 0 maj, 1 min, 2 dom7, 3 maj7, 4 min7
struct Note  { int step; int midi; int len; };  // step = eighth-note index in the loop
struct Ev    { double beat; bool on; int note; int vel; int ch; };
struct Seg   { Chord chord; int start; int len; };   // one chord inside the pattern (start/len in beats)

inline std::vector<int> intervals(int q) {
    switch (q) {
        case 1:  return {0, 3, 7};
        case 2:  return {0, 4, 7, 10};
        case 3:  return {0, 4, 7, 11};
        case 4:  return {0, 3, 7, 10};
        default: return {0, 4, 7};
    }
}
inline const char* suffix(int q) { static const char* s[] = {"", "m", "7", "maj7", "m7"}; return s[q]; }

// ---------- Popular progressions (the library) ----------
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
            int q = (suf == "maj7") ? 3 : (suf == "7") ? (up ? 2 : 4) : (up ? 0 : 1);
            return { (offs[k] + shift + 12) % 12, q };
        }
    }
    return {0, 0};
}

inline std::vector<Chord> parseProgression(const std::string& s) {
    std::istringstream ss(s);
    std::string tok; std::vector<Chord> r;
    while (ss >> tok) r.push_back(parseChord(tok));
    return r;
}

inline const std::vector<std::string>& progList(int vibe, bool minor) {
    const auto& v = vibes()[(size_t) vibe];
    return minor ? v.minor : v.major;
}

inline std::vector<Chord> buildProgression(int vibe, bool minor, int idx) {
    const auto& l = progList(vibe, minor);
    return parseProgression(l[(size_t) idx % l.size()]);
}

inline std::string chordName(int root, const Chord& c) {
    return std::string(NOTE[(root + c.offset) % 12]) + suffix(c.quality);
}
inline std::string chordNotes(int root, const Chord& c) {
    std::string s;
    for (int iv : intervals(c.quality)) s += std::string(s.empty() ? "" : " ") + NOTE[(root + c.offset + iv) % 12];
    return s;
}
inline std::string progNames(int root, const std::string& tokens) {
    std::string s;
    for (auto& c : parseProgression(tokens)) s += std::string(s.empty() ? "" : " ") + chordName(root, c);
    return s;
}

// ---------- Progression generator ----------
// Starts on the main chord (tonic), then walks through related chords using common
// chord-movement habits. The vibe adds colour (7ths for Chill/Dreamy, borrowed chords for Dark/Epic).
inline std::string sevenify(const std::string& t) {
    const size_t i = (t[0] == 'b' || t[0] == '#') ? 1 : 0;
    if (t == "V" || t == "bVII") return t + "7";
    return std::isupper((unsigned char) t[i]) ? t + "maj7" : t + "7";
}

inline std::string generatedTokens(int vibe, bool minor, unsigned seed, int count = 4) {
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
        if (count <= 1 || t.back() != tonic) break;   // don't end on the main chord, so the loop keeps moving
    }
    const std::string name = vibes()[(size_t) vibe].name;
    for (auto& s : t) {
        if (name == "Chill" || name == "Dreamy") s = sevenify(s);
        else if (!minor && name == "Dark")  { if (s == "V" && rng() % 10 < 7) s = "bVII"; else if (s == "IV" && rng() % 10 < 7) s = "iv"; }
        else if (!minor && name == "Epic")  { if (s == "V" && rng() % 10 < 4) s = "bVII"; }
    }
    std::string out;
    for (auto& s : t) out += (out.empty() ? "" : " ") + s;
    return out;
}

// Popular progression stretched/cycled to the wanted number of chords.
inline std::string popularTokens(int vibe, bool minor, int idx, int count) {
    const auto& l = progList(vibe, minor);
    std::istringstream ss(l[(size_t) idx % l.size()]);
    std::vector<std::string> base; std::string t;
    while (ss >> t) base.push_back(t);
    std::string out;
    for (int i = 0; i < std::clamp(count, 1, 8); ++i) out += (out.empty() ? "" : " ") + base[(size_t) i % base.size()];
    return out;
}

// source 0 = popular library, 1 = generated
inline std::vector<Chord> resolveChords(int source, int vibe, bool minor, int idx, int gen, int count) {
    count = std::clamp(count, 1, 8);
    return parseProgression(source == 1 ? generatedTokens(vibe, minor, (unsigned) gen, count)
                                        : popularTokens(vibe, minor, idx, count));
}

// Spread the chords over the pattern (4 or 8 bars). Chord changes land on 2-beat slots.
inline std::vector<Seg> layout(const std::vector<Chord>& ch, int bars) {
    const int n = (int) ch.size(), slots = bars * 2;
    std::vector<Seg> s;
    for (int i = 0; i < n; ++i) {
        const int a = (int) std::lround((double) i * slots / n) * 2;
        const int b = (i + 1 < n) ? (int) std::lround((double) (i + 1) * slots / n) * 2 : bars * 4;
        s.push_back({ch[(size_t) i], a, b - a});
    }
    return s;
}

inline std::vector<Seg> buildSegs(int source, int vibe, bool minor, int idx, int gen, int count, int bars) {
    return layout(resolveChords(source, vibe, minor, idx, gen, count), bars);
}

// ---------- Melody ----------
// Deterministic: same inputs + same seed = same melody.
inline std::vector<Note> makeMelody(const std::vector<Seg>& segs, int bars, int root, bool minor, float dens, unsigned seed) {
    std::mt19937 rng(seed * 2654435761u + 12345u);
    auto rnd = [&]() { return std::uniform_real_distribution<float>(0.f, 1.f)(rng); };
    static const int maj[7] = {0, 2, 4, 5, 7, 9, 11}, mnr[7] = {0, 2, 3, 5, 7, 8, 10};
    bool inScale[12] = {false};
    for (int k = 0; k < 7; ++k) inScale[((minor ? mnr[k] : maj[k]) + root) % 12] = true;
    std::vector<int> L;
    for (int m = 60; m <= 84; ++m) if (inScale[m % 12]) L.push_back(m);
    int prev = L[L.size() / 2];
    std::vector<Note> out;
    const int steps = bars * 8;
    for (int st = 0; st < steps; ++st) {
        size_t k = 0;
        for (size_t j = 0; j < segs.size(); ++j) if (st / 2 >= segs[j].start) k = j;
        const Seg& sg = segs[k];
        const bool strong = (st % 4 == 0) || (st == sg.start * 2);   // chord changes always get a note
        if (!strong && rnd() > dens) continue;
        bool inChord[12] = {false};
        for (int iv : intervals(sg.chord.quality)) inChord[(root + sg.chord.offset + iv) % 12] = true;
        int n;
        if (strong || rnd() < 0.25f) {
            std::vector<int> C;
            for (int m = 60; m <= 84; ++m) if (inChord[m % 12]) C.push_back(m);
            std::sort(C.begin(), C.end(), [&](int x, int y) { return std::abs(x - prev) < std::abs(y - prev); });
            n = C[(rnd() < 0.7f || C.size() < 2) ? 0 : 1];
        } else {
            int i = 0;
            for (size_t j = 0; j < L.size(); ++j) if (std::abs(L[j] - prev) < std::abs(L[(size_t) i] - prev)) i = (int) j;
            static const int stp[6] = {-2, -1, -1, 1, 1, 2};
            i = std::clamp(i + stp[(int) (rnd() * 6) % 6], 0, (int) L.size() - 1);
            n = L[(size_t) i];
        }
        out.push_back({st, n, 1});
        prev = n;
    }
    for (size_t i = 0; i < out.size(); ++i) {
        const int nx = (i + 1 < out.size()) ? out[i + 1].step : steps;
        out[i].len = std::min(nx - out[i].step, 4);
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

// ---------- MIDI events (plugin output + MIDI export) ----------
// human = 0..1. Adds loose timing, velocity accents, varied note lengths and a small chord strum.
// Deterministic for a given seed, so playback and exported MIDI match.
inline std::vector<Ev> buildEvents(const std::vector<Seg>& segs, const std::vector<Note>& mel, int bars, int root,
                                   bool chords, bool melody, float human, unsigned seed) {
    std::mt19937 rng(seed * 747796405u + 2891336453u);
    auto rnd = [&]() { return std::uniform_real_distribution<double>(0.0, 1.0)(rng); };
    auto jit = [&](double a) { return (rnd() - 0.5) * 2.0 * a; };
    const double L = bars * 4.0, h = std::clamp((double) human, 0.0, 1.0);
    auto vel = [&](double base, double spread) { return std::clamp((int) std::lround(base + jit(spread * h)), 1, 127); };
    std::vector<Ev> ev;

    if (chords)
        for (auto& sg : segs) {
            const int base = 48 + (root + sg.chord.offset) % 12;
            const double t0 = sg.start, tEnd = sg.start + sg.len;
            auto add = [&](int note, int v, double delay) {
                const double on = std::max(0.0, t0 + delay + jit(0.012 * h));
                double off = std::min(L - 0.01, tEnd - 0.05 + jit(0.05 * h));
                if (off < on + 0.1) off = on + 0.1;
                ev.push_back({on, true, note, v, 1});
                ev.push_back({off, false, note, 0, 1});
            };
            add(base - 12, vel(85, 10), 0.0);
            int k = 0;
            for (int iv : intervals(sg.chord.quality)) add(base + iv, vel(70, 12), 0.03 * (++k) * h);
        }

    if (melody) {
        std::vector<double> on(mel.size());
        for (size_t i = 0; i < mel.size(); ++i) on[i] = std::max(0.0, mel[i].step * 0.5 + jit(0.045 * h));
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

} // namespace fire
