#pragma once
#include "PluginProcessor.h"

namespace fireui {
inline const juce::Colour kBg1(0xff130806), kBg2(0xff2f0f07), kCard(0xff22100b), kCard2(0xff321811),
                          kLine(0xff4d2a1f), kFire(0xffff5a1f), kGold(0xffffc233),
                          kText(0xffffeee3), kMut(0xffc79a85);
}

// Pill buttons: flame gradient when "on", two-line text for progression chips.
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
    explicit FireProgressEditor(FireProgressProcessor&);
    ~FireProgressEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;
    void timerCallback() override;
    void setParam(const char* id, float v);
    int geti(const char* id) const { return (int) proc.apvts.getRawParameterValue(id)->load(); }
    void refreshSeed();                                   // picks a melody that was never used before
    void selectProgression(int source, int prog, int gen);
    void generate();

    FireLNF lnf;
    FireProgressProcessor& proc;
    juce::ComboBox rootBox, modeBox;
    juce::TextButton vibeBtn[7], popBtn[3], genBtn[3], generateBtn { "Generate" },
                     chordsBtn { "Chords" }, melodyBtn { "Melody" }, newMelody { "New melody" }, playBtn { "Play" };
    juce::Slider bpm;
    DragPad padChords, padMelody;
    std::unique_ptr<APVTS::ComboBoxAttachment> rootA, modeA;
    std::unique_ptr<APVTS::ButtonAttachment> chordsA, melodyA, playA;
    std::unique_ptr<APVTS::SliderAttachment> bpmA;

    std::vector<int> genHist;            // last generated progressions (newest first)
    juce::String statusMsg;
    juce::Rectangle<int> cardMain, cardProg, chordArea, cardMel, rollArea, footer;
};
