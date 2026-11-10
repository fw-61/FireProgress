#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <array>
#include <unordered_set>
#include "Engine.h"

class FireProgressProcessor : public juce::AudioProcessor {
public:
    // One entry of the history (raw parameter values): root, mode, vibe, source, prog, gen, seed, count, bars,
    // trad, raga, sgen, selec, sother
    using Snap = std::array<int, 14>;

    FireProgressProcessor();
    ~FireProgressProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& l) const override {
        return l.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return "Fire Progress"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*, int) override;

    juce::AudioProcessorValueTreeState apvts;
    std::atomic<double> currentBeat { -1.0 };  // position inside the pattern (beats), -1 = stopped
    std::atomic<bool> hostSynced { false };    // true when the DAW provides tempo + position
    std::atomic<double> hostBpm { 100.0 };

    // ---- message-thread helpers (used by the editor) ----
    int  freshSeed(const fire::Setup& s, const std::vector<fire::Seg>& segs);
    void trackMelody(uint64_t hash);
    bool melodyIsRepeat();
    int  usedCount();
    juce::File exportMidi(bool chordsPart);

    // ---- history / undo ----
    void recordSnapshot(const Snap& s);          // adds an entry if the pattern changed
    bool historyStep(int delta, Snap& out);      // -1 = undo, +1 = redo
    bool historyJump(int index, Snap& out);
    int  historySize();
    int  historyPos();
    Snap historyAt(int index);

private:
    struct Pattern { std::vector<fire::Ev> events; double loopBeats = 16.0; } pattern;
    void refreshPattern();
    void readKey(int* k) const;      // 17 raw parameter values

    int key[17] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    bool flush = false, wasRunning = false;
    double internalBeat = 0.0, lastEnd = -1.0, sr = 44100.0;
    juce::Synthesiser chordSynth, leadSynth;
    std::atomic<float> *pRoot, *pMode, *pVibe, *pProg, *pSeed, *pChords, *pMelody, *pBpm, *pRun, *pSource, *pGen,
                       *pBars, *pCount, *pHuman, *pCSound, *pMSound, *pTrad, *pRaga, *pSG, *pSE, *pSO;

    juce::CriticalSection usedLock;            // melody memory (message thread only)
    std::unordered_set<uint64_t> used;
    uint64_t lastHash = 0;
    bool repeat = false;

    juce::CriticalSection histLock;            // history (message thread only)
    std::vector<Snap> hist;
    int histPos = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireProgressProcessor)
};
