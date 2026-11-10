#pragma once
#include "PluginProcessor.h"

namespace fireui {
inline const juce::Colour kBg1(0xff130806), kBg2(0xff2f0f07), kCard(0xff22100b), kCard2(0xff321811),
                          kLine(0xff4d2a1f), kFire(0xffff5a1f), kGold(0xffffc233),
                          kText(0xffffeee3), kMut(0xffc79a85);
}

// Pill buttons: flame gradient when "on", two-line text for chips.
class FireLNF : public juce::LookAndFeel_V4 {
public:
    FireLNF();
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText(juce::Graphics&, juce::TextButton&, bool, bool) override;
};

// A pad you can drag into the DAW (it drags a .mid file). A plain click saves the file to Documents/FireProgress.
class DragPad : public juce::Component {
public:
    std::function<juce::File()> makeFile;
    std::function<void(const juce::String&)> onMessage;
    juce::String label;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override { dragged = false; }
    void mouseDrag(const juce::MouseEvent&) override;
    void mouseUp(const juce::MouseEvent&) override;
    void mouseEnter(const juce::MouseEvent&) override { hover = true; repaint(); }
    void mouseExit(const juce::MouseEvent&) override { hover = false; repaint(); }

private:
    bool dragged = false, hover = false;
};

class FireProgressEditor : public juce::AudioProcessorEditor, private juce::Timer {
public:
    using Snap = FireProgressProcessor::Snap;

    explicit FireProgressEditor(FireProgressProcessor&);
    ~FireProgressEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress&) override;   // Ctrl+Z = undo, Ctrl+Shift+Z / Ctrl+Y = redo
    void mouseDown(const juce::MouseEvent&) override { grabKeyboardFocus(); }

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    void setParam(const char* id, float v);
    int geti(const char* id) const { return (int) proc.apvts.getRawParameterValue(id)->load(); }
    void syncCombo(juce::ComboBox&, const char* id);

    fire::Setup setup() const;                            // current settings as an engine Setup
    static fire::Setup setupFromSnap(const Snap&);

    void refreshSeed();                                   // picks a melody that was never used before
    void selectProgression(int source, int prog, int gen);
    void newGenerated();                                  // new generated progression (no melody refresh)
    void generate();
    void applyStyleDefaults();                            // tempo + sounds suggested by the chosen style / raga
    void paintInfo(juce::Graphics&, const fire::Setup&);

    Snap snapshot() const;
    void applySnapshot(const Snap&);
    void undo();
    void redo();
    void jumpTo(int index);
    void showHistoryList();
    juce::String labelOf(const fire::Setup&) const;       // e.g. "Japanese + Lo-fi - 8 bars"
    juce::String describe(const Snap&) const;             // two lines: chords / label
    juce::String describeLine(const Snap&) const;         // one line for the popup list
    juce::String chipText(const fire::Setup&, const std::string& tokens) const;

    FireLNF lnf;
    FireProgressProcessor& proc;
    juce::ComboBox rootBox, modeBox, barsBox, countBox;                        // bound to parameters
    juce::ComboBox tradBox, ragaBox, genreBox, electroBox, otherBox;          // handled by hand (side effects)
    juce::TextButton vibeBtn[7], popBtn[3], genBtn[3], histBtn[4], cTone[3], mTone[3],
                     generateBtn { "Generate" }, undoBtn { "Undo" }, redoBtn { "Redo" }, listBtn { "All" },
                     chordsBtn { "Chords" }, melodyBtn { "Melody" }, newMelody { "New melody" }, playBtn { "Play" };
    juce::Slider bpm, humanSlider;
    DragPad padChords, padMelody;
    std::unique_ptr<APVTS::ComboBoxAttachment> rootA, modeA, barsA, countA;
    std::unique_ptr<APVTS::ButtonAttachment> chordsA, melodyA, playA;
    std::unique_ptr<APVTS::SliderAttachment> bpmA, humanA;

    std::vector<int> genHist;            // last generated progressions (newest first)
    int histIdx[4] = { -1, -1, -1, -1 };
    juce::String statusMsg;
    juce::Rectangle<int> cardStyle, cardMain, cardProg, cardHist, chordArea, cardMel, cardInfo, rollArea, footer,
                         lblTrad, lblRaga, lblGenre, lblElec, lblOther, lblCT, lblMT, lblH;
};
