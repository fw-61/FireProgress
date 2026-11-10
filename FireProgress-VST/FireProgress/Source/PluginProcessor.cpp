#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace {
struct Sound : juce::SynthesiserSound {
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

// pad = soft saw (chords), lead = triangle (melody)
struct Voice : juce::SynthesiserVoice {
    explicit Voice(bool isPad) : pad(isPad) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<Sound*>(s) != nullptr; }
    void startNote(int note, float vel, juce::SynthesiserSound*, int) override {
        const double sr = getSampleRate();
        inc = juce::MidiMessage::getMidiNoteInHertz(note) / sr;
        level = vel * (pad ? 0.20f : 0.28f);
        const double fc = pad ? 1200.0 : 4000.0;
        coef = (float) (1.0 - std::exp(-juce::MathConstants<double>::twoPi * fc / sr));
        phase = 0.0; lp = 0.0f;
        adsr.setSampleRate(sr);
        adsr.setParameters(pad ? juce::ADSR::Parameters(0.12f, 0.3f, 0.7f, 0.45f)
                               : juce::ADSR::Parameters(0.01f, 0.2f, 0.5f, 0.15f));
        adsr.noteOn();
    }
    void stopNote(float, bool allowTailOff) override {
        if (allowTailOff) adsr.noteOff(); else { adsr.reset(); clearCurrentNote(); }
    }
    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}
    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override {
        if (!isVoiceActive()) return;
        for (int i = 0; i < num; ++i) {
            const float raw = pad ? (float) (2.0 * phase - 1.0) : (float) (4.0 * std::abs(phase - 0.5) - 1.0);
            lp += coef * (raw - lp);
            const float s = lp * level * adsr.getNextSample();
            for (int c = 0; c < out.getNumChannels(); ++c) out.addSample(c, start + i, s);
            phase += inc; if (phase >= 1.0) phase -= 1.0;
            if (!adsr.isActive()) { clearCurrentNote(); break; }
        }
    }
    bool pad; double phase = 0, inc = 0; float level = 0, lp = 0, coef = 0.1f; juce::ADSR adsr;
};

juce::AudioProcessorValueTreeState::ParameterLayout makeLayout() {
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> p;
    StringArray notes, vs;
    for (auto n : fire::NOTE) notes.add(n);
    for (auto& v : fire::vibes()) vs.add(v.name);
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"root", 1}, "Root", notes, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"mode", 1}, "Mode", StringArray{"Major", "Minor"}, 0));
    p.push_back(std::make_unique<AudioParameterChoice>(ParameterID{"vibe", 1}, "Vibe", vs, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"source", 1}, "Progression source", 0, 1, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"prog", 1}, "Popular progression", 0, 2, 0));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"gen", 1}, "Generated progression", 0, 9999, 1));
    p.push_back(std::make_unique<AudioParameterInt>(ParameterID{"seed", 1}, "Melody seed", 0, 9999, 1));
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
    pGen = apvts.getRawParameterValue("gen");
    for (int i = 0; i < 8; ++i) { pad.addVoice(new Voice(true)); lead.addVoice(new Voice(false)); }
    pad.addSound(new Sound()); lead.addSound(new Sound());
}

void FireProgressProcessor::prepareToPlay(double sampleRate, int) {
    sr = sampleRate;
    pad.setCurrentPlaybackSampleRate(sr);
    lead.setCurrentPlaybackSampleRate(sr);
    internalBeat = 0.0; lastEnd = -1.0; wasRunning = false;
}

void FireProgressProcessor::refreshPattern() {
    const int k[9] = { (int) pRoot->load(), (int) pMode->load(), (int) pVibe->load(), (int) pProg->load(),
                       (int) pSeed->load(), pChords->load() > 0.5f ? 1 : 0, pMelody->load() > 0.5f ? 1 : 0,
                       (int) pSource->load(), (int) pGen->load() };
    if (std::equal(k, k + 9, key)) return;
    std::copy(k, k + 9, key);
    flush = true;

    const bool minor = (k[1] == 1);
    const auto prog = fire::resolveProgression(k[7], k[2], minor, k[3], k[8]);
    const auto mel = fire::makeMelody(prog, k[0], minor, fire::vibes()[(size_t) k[2]].density, (unsigned) k[4]);
    pattern.loopBeats = (double) prog.size() * 4.0;
    pattern.events = fire::buildEvents(prog, mel, k[0], k[5] != 0, k[6] != 0);
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
        currentChord.store((int) (std::fmod(a, L) / 4.0));
        lastEnd = b;
        if (!host) internalBeat += bb;
    } else {
        currentChord.store(-1);
        lastEnd = -1.0;
        if (!host) internalBeat = 0.0;
    }
    wasRunning = running;

    juce::MidiBuffer cb, mb;
    for (const auto m : gen) {
        auto msg = m.getMessage();
        (msg.getChannel() == 1 ? cb : mb).addEvent(msg, m.samplePosition);
    }
    pad.renderNextBlock(buf, cb, 0, n);
    lead.renderNextBlock(buf, mb, 0, n);
    midi.addEvents(gen, 0, n, 0);   // chords on MIDI ch 1, melody on ch 2
}

// ------------------------------------------------------------------ melody memory
int FireProgressProcessor::freshSeed(const std::vector<fire::Chord>& prog, int root, bool minor, float dens) {
    auto& rnd = juce::Random::getSystemRandom();
    const juce::ScopedLock sl(usedLock);
    int s = 0;
    for (int i = 0; i < 400; ++i) {
        s = rnd.nextInt(10000);
        if (used.count(fire::melodyHash(fire::makeMelody(prog, root, minor, dens, (unsigned) s))) == 0) return s;
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

// ------------------------------------------------------------------ MIDI export (drag into the DAW)
juce::File FireProgressProcessor::exportMidi(bool chordsPart) {
    const int root = (int) pRoot->load(), vibe = (int) pVibe->load();
    const bool minor = pMode->load() > 0.5f;
    const auto prog = fire::resolveProgression((int) pSource->load(), vibe, minor, (int) pProg->load(), (int) pGen->load());
    const auto mel = fire::makeMelody(prog, root, minor, fire::vibes()[(size_t) vibe].density, (unsigned) pSeed->load());
    const auto ev = fire::buildEvents(prog, mel, root, chordsPart, !chordsPart);
    const double bpm = hostSynced.load() ? hostBpm.load() : (double) pBpm->load();

    juce::MidiMessageSequence tempoTrack, seq;
    tempoTrack.addEvent(juce::MidiMessage::tempoMetaEvent((int) (60000000.0 / juce::jmax(20.0, bpm))));
    for (auto& e : ev) {
        auto m = e.on ? juce::MidiMessage::noteOn(1, e.note, (juce::uint8) e.vel) : juce::MidiMessage::noteOff(1, e.note);
        m.setTimeStamp(e.beat * 480.0);
        seq.addEvent(m);
    }
    auto end = juce::MidiMessage::endOfTrack();
    end.setTimeStamp((double) prog.size() * 4.0 * 480.0);
    seq.addEvent(end);

    juce::MidiFile file;
    file.setTicksPerQuarterNote(480);
    file.addTrack(tempoTrack);
    file.addTrack(seq);

    auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("FireProgress");
    dir.createDirectory();
    const juce::String name = juce::String("FireProgress_") + (chordsPart ? "Chords" : "Melody") + "_"
        + juce::String(fire::NOTE[root]).replace("#", "s") + (minor ? "min" : "maj") + "_"
        + juce::String(fire::vibes()[(size_t) vibe].name) + "_"
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
    if (auto xml = st.createXml()) copyXmlToBinary(*xml, dest);
}

void FireProgressProcessor::setStateInformation(const void* data, int size) {
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(apvts.state.getType())) {
            apvts.replaceState(juce::ValueTree::fromXml(*xml));
            const juce::ScopedLock sl(usedLock);
            used.clear();
            for (auto& t : juce::StringArray::fromTokens(apvts.state.getProperty("used").toString(), ",", ""))
                if (t.isNotEmpty()) used.insert((uint64_t) t.getHexValue64());
            lastHash = (uint64_t) apvts.state.getProperty("last").toString().getHexValue64();
            repeat = false;
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new FireProgressProcessor(); }
