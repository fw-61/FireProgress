#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
struct Sound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// Built-in sounds: 0 = Piano, 1 = Synth, 2 = Pad. Each layer (chords / melody) reads its own selector.
struct Voice : juce::SynthesiserVoice {
    Voice(bool chordLayer, const std::atomic<float>* snd) : chord(chordLayer), sound(snd) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }

    void startNote(int note, float vel, juce::SynthesiserSound*, int) override {
        sr = getSampleRate();
        mode = juce::jlimit(0, 2, (int) sound->load());
        const double f = juce::MidiMessage::getMidiNoteInHertz(note);
        const double twoPi = juce::MathConstants<double>::twoPi;
        const float vs = 0.4f + 0.6f * vel;
        lp = 0.0f;
        if (mode == 0) {   // piano: decaying partials, short damper on release
            static const double amps[6] = {1.0, 0.55, 0.32, 0.2, 0.12, 0.07};
            const double tau1 = juce::jlimit(0.35, 2.4, 2.6 - note * 0.02);
            for (int k = 0; k < 6; ++k) {
                ph[k] = 0.0;
                inc[k] = twoPi * f * (k + 1) * (1.0 + 0.0003 * k * k) / sr;
                amp[k] = (float) (amps[k] * (0.5 + 0.5 * vel));
                dec[k] = (float) std::exp(-1.0 / (sr * tau1 / (1.0 + 0.7 * k)));
            }
            level = (chord ? 0.15f : 0.20f) * vs;
            adsr.setParameters(juce::ADSR::Parameters(0.002f, 0.05f, 1.0f, 0.25f));
        } else {           // synth (2 detuned saws) or pad (3 detuned saws, slow)
            const double det[3] = {0.0, 7.0, -7.0};   // cents
            nSaw = (mode == 1) ? 2 : 3;
            for (int k = 0; k < 3; ++k) { ph[k] = 0.17 * k; inc[k] = f * std::pow(2.0, det[k] / 1200.0) / sr; }
            const double fc = (mode == 1) ? (chord ? 2200.0 : 4500.0) : 1100.0;
            coef = (float) (1.0 - std::exp(-twoPi * fc / sr));
            level = ((mode == 1) ? (chord ? 0.13f : 0.18f) : (chord ? 0.12f : 0.15f)) * vs;
            if (mode == 1)
                adsr.setParameters(chord ? juce::ADSR::Parameters(0.02f, 0.3f, 0.7f, 0.25f)
                                         : juce::ADSR::Parameters(0.008f, 0.2f, 0.55f, 0.18f));
            else
                adsr.setParameters(chord ? juce::ADSR::Parameters(0.25f, 0.4f, 0.8f, 0.8f)
                                         : juce::ADSR::Parameters(0.12f, 0.3f, 0.8f, 0.6f));
        }
        adsr.setSampleRate(sr);
        adsr.noteOn();
    }
    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) adsr.noteOff(); else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        const double twoPi = juce::MathConstants<double>::twoPi;
        for (int i = 0; i < num; ++i) {
            float s = 0.0f;
            if (mode == 0) {
                for (int k = 0; k < 6; ++k) {
                    s += amp[k] * (float) std::sin(ph[k]);
                    ph[k] += inc[k]; if (ph[k] > twoPi) ph[k] -= twoPi;
                    amp[k] *= dec[k];
                }
            } else {
                for (int k = 0; k < nSaw; ++k) {
                    s += (float) (2.0 * ph[k] - 1.0);
                    ph[k] += inc[k]; if (ph[k] >= 1.0) ph[k] -= 1.0;
                }
                s /= (float) nSaw;
                lp += coef * (s - lp);
                s = lp;
            }
            s *= level * adsr.getNextSample();
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample(c, start + i, s);
            if (!adsr.isActive() || (mode == 0 && amp[0] < 0.0005f)) { clearCurrentNote(); break; }
        }
    }

    bool chord;
    const std::atomic<float>* sound;
    int mode = 0, nSaw = 2;
    double sr = 44100.0, ph[6] = {0, 0, 0, 0, 0, 0}, inc[6] = {0, 0, 0, 0, 0, 0};
    float amp[6] = {0, 0, 0, 0, 0, 0}, dec[6] = {1, 1, 1, 1, 1, 1};
    float level = 0.0f, lp = 0.0f, coef = 0.1f;
    juce::ADSR adsr;
};

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    StringArray notes, vs, counts, tones{"Piano", "Synth", "Pad"};
    for (auto n : fire::NOTE) notes.add(n);
    for (auto& v : fire::vibes()) vs.add(v.name);
    for (int i = 1; i <= 8; ++i) counts.add(String(i) + (i == 1 ? " chord" : " chords"));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"root", 1}, "Root", notes, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, "Mode", StringArray{"Major", "Minor"}, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"vibe", 1}, "Vibe", vs, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"source", 1}, "Progression source", 0, 1, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"prog", 1}, "Popular progression", 0, 2, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"gen", 1}, "Generated progression", 0, 9999, 1));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"seed", 1}, "Melody seed", 0, 9999, 1));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"bars", 1}, "Pattern size", StringArray{"4 bars", "8 bars"}, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"count", 1}, "Chords per pattern", counts, 3));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"human", 1}, "Humanize", NormalisableRange<float>(0.f, 100.f, 1.f), 55.f));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"csound", 1}, "Chord sound", tones, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"msound", 1}, "Melody sound", tones, 1));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"chords", 1}, "Chords", true));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"melody", 1}, "Melody", true));
    p.push_back(std::make_unique<AudioParameterFloat>(ParameterID{"bpm", 1}, "Tempo (standalone)",
                                                      NormalisableRange<float>(60.f, 160.f, 1.f), 100.f));
    p.push_back(std::make_unique<AudioParameterBool>(ParameterID{"run", 1}, "Play (standalone)", false));
    return { p.begin(), p.end() };
}
} // namespace

FireProgressProcessor::FireProgressProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "STATE", makeLayout()) {
    pRoot = apvts.getRawParameterValue("root");     pMode = apvts.getRawParameterValue("mode");
    pVibe = apvts.getRawParameterValue("vibe");     pProg = apvts.getRawParameterValue("prog");
    pSeed = apvts.getRawParameterValue("seed");     pChords = apvts.getRawParameterValue("chords");
    pMelody = apvts.getRawParameterValue("melody"); pBpm = apvts.getRawParameterValue("bpm");
    pRun = apvts.getRawParameterValue("run");       pSource = apvts.getRawParameterValue("source");
    pGen = apvts.getRawParameterValue("gen");       pBars = apvts.getRawParameterValue("bars");
    pCount = apvts.getRawParameterValue("count");   pHuman = apvts.getRawParameterValue("human");
    pCSound = apvts.getRawParameterValue("csound"); pMSound = apvts.getRawParameterValue("msound");
    for (int i = 0; i < 12; ++i) chordSynth.addVoice(new Voice(true, pCSound));
    for (int i = 0; i < 8; ++i)  leadSynth.addVoice(new Voice(false, pMSound));
    chordSynth.addSound(new Sound()); leadSynth.addSound(new Sound());
}

void FireProgressProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate;
    chordSynth.setCurrentPlaybackSampleRate(sr);
    leadSynth.setCurrentPlaybackSampleRate(sr);
    internalBeat = 0.0; lastEnd = -1.0; wasRunning = false;
}

void FireProgressProcessor::refreshPattern() {
    const int k[12] = { (int) pRoot->load(), (int) pMode->load(), (int) pVibe->load(), (int) pProg->load(),
                        (int) pSeed->load(), pChords->load() > 0.5f ? 1 : 0, pMelody->load() > 0.5f ? 1 : 0,
                        (int) pSource->load(), (int) pGen->load(), (int) pBars->load(), (int) pCount->load(),
                        (int) pHuman->load() };
    if (std::equal(k, k + 12, key)) return;
    std::copy(k, k + 12, key);
    flush = true;

    const bool minor = (k[1] == 1);
    const int bars = (k[9] == 1) ? 8 : 4;
    const auto segs = fire::buildSegs(k[7], k[2], minor, k[3], k[8], k[10] + 1, bars);
    const auto mel = fire::makeMelody(segs, bars, k[0], minor, fire::vibes()[(size_t) k[2]].density, (unsigned) k[4]);
    pattern.loopBeats = bars * 4.0;
    pattern.events = fire::buildEvents(segs, mel, bars, k[0], k[5] != 0, k[6] != 0, (float) k[11] / 100.0f, (unsigned) k[4]);
}

void FireProgressProcessor::processBlock(juce::AudioBuffer<float>& buf, juce::MidiBuffer& midi) {
    juce::ScopedNoDenormals noDenormals;
    const int n = buf.getNumSamples();
    buf.clear(); midi.clear();
    refreshPattern();

    // ---- Tempo + transport: follow the DAW automatically, fall back to our own clock ----
    double bpm = pBpm->load(), beat = 0.0;
    bool host = false, running = false;
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition()) {
            if (auto t = pos->getBpm()) bpm = *t;
            if (auto p = pos->getPpqPosition()) { host = true; running = pos->getIsPlaying(); beat = *p; }
        }
    if (!host) { running = pRun->load() > 0.5f; beat = internalBeat; }
    if (beat < 0.0) running = false;
    if (bpm < 20.0) bpm = 100.0;
    hostSynced.store(host);
    hostBpm.store(bpm);

    juce::MidiBuffer gen;
    const bool jumped = wasRunning && running && lastEnd >= 0.0 && std::abs(beat - lastEnd) > 0.1;
    if (flush || jumped || (wasRunning && !running))
        for (int ch = 1; ch <= 2; ++ch) gen.addEvent(juce::MidiMessage::allNotesOff(ch), 0);
    flush = false;

    if (running) {
        const double bb = n / sr * bpm / 60.0, a = beat, b = beat + bb, L = pattern.loopBeats;
        for (long lp = (long) std::floor(a / L); lp <= (long) std::floor((b - 1e-9) / L); ++lp)
            for (auto& e : pattern.events) {
                const double t = (double) lp * L + e.beat;
                if (t < a || t >= b) continue;
                const int off = juce::jlimit(0, n - 1, (int) ((t - a) * 60.0 / bpm * sr));
                gen.addEvent(e.on ? juce::MidiMessage::noteOn(e.ch, e.note, (juce::uint8) e.vel)
                                  : juce::MidiMessage::noteOff(e.ch, e.note), off);
            }
        currentBeat.store(std::fmod(a, L));
        lastEnd = b;
        if (!host) internalBeat += bb;
    } else {
        currentBeat.store(-1.0);
        lastEnd = -1.0;
        if (!host) internalBeat = 0.0;
    }
    wasRunning = running;

    juce::MidiBuffer cb, mb;
    for (const auto m : gen) {
        auto msg = m.getMessage();
        (msg.getChannel() == 1 ? cb : mb).addEvent(msg, m.samplePosition);
    }
    chordSynth.renderNextBlock(buf, cb, 0, n);
    leadSynth.renderNextBlock(buf, mb, 0, n);
    midi.addEvents(gen, 0, n, 0);   // chords on MIDI ch 1, melody on ch 2
}

// ------------------------------------------------------------------ melody memory
int FireProgressProcessor::freshSeed(const std::vector<fire::Seg>& segs, int bars, int root, bool minor, float dens) {
    auto& rnd = juce::Random::getSystemRandom();
    const juce::ScopedLock sl(usedLock);
    int s = 0;
    for (int i = 0; i < 400; ++i) {
        s = rnd.nextInt(10000);
        if (used.count(fire::melodyHash(fire::makeMelody(segs, bars, root, minor, dens, (unsigned) s))) == 0) return s;
    }
    return s;   // everything we tried was already used; the UI will say "Used before"
}

void FireProgressProcessor::trackMelody(uint64_t h) {
    const juce::ScopedLock sl(usedLock);
    if (h == lastHash) return;
    lastHash = h;
    if (used.size() > 5000) used.clear();
    repeat = !used.insert(h).second;
}
bool FireProgressProcessor::melodyIsRepeat() { const juce::ScopedLock sl(usedLock); return repeat; }
int  FireProgressProcessor::usedCount()      { const juce::ScopedLock sl(usedLock); return (int) used.size(); }

// ------------------------------------------------------------------ history / undo
void FireProgressProcessor::recordSnapshot(const Snap& s) {
    const juce::ScopedLock sl(histLock);
    if (histPos >= 0 && histPos < (int) hist.size() && hist[(size_t) histPos] == s) return;
    hist.push_back(s);
    if (hist.size() > 100) hist.erase(hist.begin());
    histPos = (int) hist.size() - 1;
}
bool FireProgressProcessor::historyJump(int index, Snap& out) {
    const juce::ScopedLock sl(histLock);
    if (index < 0 || index >= (int) hist.size()) return false;
    histPos = index; out = hist[(size_t) index];
    return true;
}
bool FireProgressProcessor::historyStep(int delta, Snap& out) {
    int target;
    { const juce::ScopedLock sl(histLock); target = histPos + delta; }
    return historyJump(target, out);
}
int FireProgressProcessor::historySize() { const juce::ScopedLock sl(histLock); return (int) hist.size(); }
int FireProgressProcessor::historyPos()  { const juce::ScopedLock sl(histLock); return histPos; }
FireProgressProcessor::Snap FireProgressProcessor::historyAt(int index) {
    const juce::ScopedLock sl(histLock);
    if (index < 0 || index >= (int) hist.size()) return Snap{};
    return hist[(size_t) index];
}

// ------------------------------------------------------------------ MIDI export (drag into the DAW)
juce::File FireProgressProcessor::exportMidi(bool chordsPart) {
    const int root = (int) pRoot->load(), vibe = (int) pVibe->load();
    const bool minor = pMode->load() > 0.5f;
    const int bars = ((int) pBars->load() == 1) ? 8 : 4;
    const auto segs = fire::buildSegs((int) pSource->load(), vibe, minor, (int) pProg->load(), (int) pGen->load(),
                                      (int) pCount->load() + 1, bars);
    const unsigned seed = (unsigned) pSeed->load();
    const auto mel = fire::makeMelody(segs, bars, root, minor, fire::vibes()[(size_t) vibe].density, seed);
    const auto ev = fire::buildEvents(segs, mel, bars, root, chordsPart, !chordsPart, pHuman->load() / 100.0f, seed);
    const double bpm = hostSynced.load() ? hostBpm.load() : (double) pBpm->load();

    juce::MidiMessageSequence tempoTrack, seq;
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent((int) (60000000.0 / juce::jmax(20.0, bpm))));
    for (auto& e : ev) {
        auto m = e.on ? juce::MidiMessage::noteOn(1, e.note, (juce::uint8) e.vel) : juce::MidiMessage::noteOff(1, e.note);
        m.setTimeStamp(e.beat * 480.0);
        seq.addEvent(m);
    }
    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp((double) bars * 4.0 * 480.0);
    seq.addEvent(end);

    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(tempoTrack);
    file.addTrack(seq);

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FireProgress");
    dir.createDirectory();
    const juce::String name = juce::String("FireProgress_") + (chordsPart ? "Chords" : "Melody") + "_"
        + juce::String(fire::NOTE[root]).replace("#", "s") + (minor ? "min" : "maj") + "_"
        + juce::String(fire::vibes()[(size_t) vibe].name) + "_" + juce::String(bars) + "bars_"
        + juce::String(juce::Time::getCurrentTime().toMilliseconds() % 100000) + ".mid";
    auto out = dir.getChildFile(name);
    out.deleteFile();
    juce::FileOutputStream os(out);
    if (os.openedOk()) file.writeTo(os);
    return out;
}

// ------------------------------------------------------------------ state
juce::AudioProcessorEditor* FireProgressProcessor::createEditor() { return new FireProgressEditor(*this); }

void FireProgressProcessor::getStateInformation(juce::MemoryBlock& dest) {
    auto st = apvts.copyState();
    {
        const juce::ScopedLock sl(usedLock);
        juce::String s; int count = 0;
        for (auto h : used) { if (++count > 3000) break; s << juce::String::toHexString((juce::int64) h) << ","; }
        st.setProperty("used", s, nullptr);
        st.setProperty("last", juce::String::toHexString((juce::int64) lastHash), nullptr);
    }
    {
        const juce::ScopedLock sl(histLock);
        juce::String h;
        for (auto& e : hist) {
            for (size_t i = 0; i < e.size(); ++i) h << e[i] << (i + 1 < e.size() ? "," : "");
            h << ";";
        }
        st.setProperty("hist", h, nullptr);
        st.setProperty("hpos", histPos, nullptr);
    }
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void FireProgressProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            {
                const juce::ScopedLock sl(usedLock);
                used.clear();
                for (auto& t : juce::StringArray::fromTokens(apvts.state.getProperty("used").toString(), ",", ""))
                    if (t.isNotEmpty()) used.insert((uint64_t) t.getHexValue64());
                lastHash = (uint64_t) apvts.state.getProperty("last").toString().getHexValue64();
                repeat = false;
            }
            {
                const juce::ScopedLock sl(histLock);
                hist.clear();
                for (auto& e : juce::StringArray::fromTokens(apvts.state.getProperty("hist").toString(), ";", "")) {
                    auto f = juce::StringArray::fromTokens(e, ",", "");
                    if (f.size() != 9) continue;
                    Snap s;
                    for (int i = 0; i < 9; ++i) s[(size_t) i] = f[i].getIntValue();
                    hist.push_back(s);
                }
                histPos = juce::jlimit(-1, (int) hist.size() - 1, (int) apvts.state.getProperty("hpos"));
            }
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FireProgressProcessor(); }
