#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include <unordered_set>
#include "Engine.h"

class FireProgressProcessor : public juce::AudioProcessor {
public:
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
    std::atomic<int> currentChord { -1 };      // read by the editor
    std::atomic<bool> hostSynced { false };    // true when the DAW provides tempo + position
    std::atomic<double> hostBpm { 100.0 };

    // ---- message-thread helpers (used by the editor) ----
    int  freshSeed(const std::vector<fire::Chord>& prog, int root, bool minor, float density); // melody seed never used before (if possible)
    void trackMelody(uint64_t hash);   // remembers the melody that is currently shown
    bool melodyIsRepeat();             // true if the shown melody was used before
    int  usedCount();
    juce::File exportMidi(bool chordsPart);   // writes a .mid file (chords or melody) at the current tempo

private:
    struct Pattern { std::vector<fire::Ev> events; double loopBeats = 16.0; } pattern;
    void refreshPattern();

    int key[9] = { -1, -1, -1, -1, -1, -1, -1, -1, -1 };
    bool flush = false, wasRunning = false;
    double internalBeat = 0.0, lastEnd = -1.0, sr = 44100.0;
    juce::Synthesiser pad, lead;
    std::atomic<float> *pRoot, *pMode, *pVibe, *pProg, *pSeed, *pChords, *pMelody, *pBpm, *pRun, *pSource, *pGen;

    juce::CriticalSection usedLock;            // only the message thread touches the melody memory
    std::unordered_set<uint64_t> used;
    uint64_t lastHash = 0;
    bool repeat = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FireProgressProcessor)
};
