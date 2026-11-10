#include "PluginEditor.h"
using namespace juce;
using namespace fireui;

// ------------------------------------------------------------------ look and feel
FireLNF::FireLNF() {
    setColour(ComboBox::backgroundColourId, kCard2);
    setColour(ComboBox::textColourId, kText);
    setColour(ComboBox::outlineColourId, kLine);
    setColour(ComboBox::arrowColourId, kGold);
    setColour(PopupMenu::backgroundColourId, kCard);
    setColour(PopupMenu::textColourId, kText);
    setColour(PopupMenu::highlightedBackgroundColourId, kFire);
    setColour(PopupMenu::highlightedTextColourId, kBg1);
    setColour(Slider::thumbColourId, kGold);
    setColour(Slider::trackColourId, kFire);
    setColour(Slider::backgroundColourId, kLine);
    setColour(Slider::textBoxTextColourId, kText);
    setColour(Slider::textBoxOutlineColourId, kLine);
}

void FireLNF::drawButtonBackground(Graphics& g, Button& b, const Colour&, bool hl, bool down) {
    auto r = b.getLocalBounds().toFloat().reduced(0.5f);
    const bool on = b.getToggleState();
    if (on) g.setGradientFill(ColourGradient(kFire, r.getX(), r.getY(), kGold, r.getRight(), r.getBottom(), false));
    else    g.setColour(down ? kLine : (hl ? kCard2.brighter(0.15f) : kCard2));
    g.fillRoundedRectangle(r, 10.f);
    if (!on) { g.setColour(kLine); g.drawRoundedRectangle(r, 10.f, 1.f); }
    if (!b.isEnabled()) { g.setColour(Colours::black.withAlpha(0.45f)); g.fillRoundedRectangle(r, 10.f); }
}

void FireLNF::drawButtonText(Graphics& g, TextButton& b, bool, bool) {
    g.setColour(b.getToggleState() ? kBg1 : kText);
    g.setFont(Font(12.5f, Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds().reduced(4, 2), Justification::centred, 2);
}

// ------------------------------------------------------------------ drag pad
void DragPad::paint(Graphics& g) {
    auto r = getLocalBounds().toFloat().reduced(0.5f);
    g.setColour(hover ? kCard2.brighter(0.2f) : kCard2);
    g.fillRoundedRectangle(r, 10.f);
    g.setColour(kFire);
    g.drawRoundedRectangle(r, 10.f, 1.5f);
    g.setColour(kText);
    g.setFont(Font(12.5f, Font::bold));
    g.drawText(label, getLocalBounds(), Justification::centred);
}

void DragPad::mouseDrag(const MouseEvent& e) {
    if (dragged || e.getDistanceFromDragStart() < 6) return;
    dragged = true;
    if (makeFile) {
        auto f = makeFile();
        if (f.existsAsFile())
            DragAndDropContainer::performExternalDragDropOfFiles(StringArray(f.getFullPathName()), false, this);
    }
}

void DragPad::mouseUp(const MouseEvent& e) {
    if (!dragged && e.getDistanceFromDragStart() < 6 && makeFile) {
        auto f = makeFile();
        auto dir = File::getSpecialLocation(File::userDocumentsDirectory).getChildFile("FireProgress");
        dir.createDirectory();
        auto out = dir.getChildFile(f.getFileName());
        if (f.copyFileTo(out) && onMessage) onMessage("Saved: " + out.getFullPathName());
    }
    dragged = false;
}

// ------------------------------------------------------------------ editor
FireProgressEditor::FireProgressEditor(FireProgressProcessor& p) : AudioProcessorEditor(&p), proc(p) {
    setLookAndFeel(&lnf);
    setSize(1000, 500);

    int id = 1;
    for (auto n : fire::NOTE) rootBox.addItem(n, id++);
    modeBox.addItem("Major", 1); modeBox.addItem("Minor", 2);
    addAndMakeVisible(rootBox); addAndMakeVisible(modeBox);

    for (int i = 0; i < 7; ++i) {
        vibeBtn[i].setButtonText(fire::vibes()[(size_t) i].name);
        vibeBtn[i].onClick = [this, i] {
            setParam("vibe", (float) i); setParam("source", 0.f); setParam("prog", 0.f); refreshSeed();
        };
        addAndMakeVisible(vibeBtn[i]);
    }
    for (int i = 0; i < 3; ++i) {
        popBtn[i].onClick = [this, i] { selectProgression(0, i, geti("gen")); };
        genBtn[i].onClick = [this, i] { if (i < (int) genHist.size()) selectProgression(1, 0, genHist[(size_t) i]); };
        addAndMakeVisible(popBtn[i]); addChildComponent(genBtn[i]);
    }
    generateBtn.setToggleState(true, dontSendNotification);
    generateBtn.onClick = [this] { generate(); };
    newMelody.setToggleState(true, dontSendNotification);
    newMelody.onClick = [this] { refreshSeed(); };
    for (auto* b : { &generateBtn, &chordsBtn, &melodyBtn, &newMelody, &playBtn }) addAndMakeVisible(b);

    padChords.label = "Drag chords MIDI";
    padMelody.label = "Drag melody MIDI";
    padChords.makeFile = [this] { return proc.exportMidi(true); };
    padMelody.makeFile = [this] { return proc.exportMidi(false); };
    padChords.onMessage = padMelody.onMessage = [this](const String& m) { statusMsg = m; repaint(); };
    padChords.setMouseCursor(MouseCursor::DraggingHandCursor);
    padMelody.setMouseCursor(MouseCursor::DraggingHandCursor);
    addAndMakeVisible(padChords); addAndMakeVisible(padMelody);

    bpm.setSliderStyle(Slider::LinearHorizontal);
    bpm.setTextBoxStyle(Slider::TextBoxRight, false, 70, 22);
    bpm.setTextValueSuffix(" BPM");
    addAndMakeVisible(bpm);

    rootA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "root", rootBox);
    modeA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "mode", modeBox);
    chordsA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "chords", chordsBtn);
    melodyA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "melody", melodyBtn);
    playA   = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "run", playBtn);
    bpmA    = std::make_unique<APVTS::SliderAttachment>(proc.apvts, "bpm", bpm);

    if (geti("source") == 1) genHist.push_back(geti("gen"));
    startTimerHz(30);
}

FireProgressEditor::~FireProgressEditor() { stopTimer(); setLookAndFeel(nullptr); }

void FireProgressEditor::setParam(const char* id, float v) {
    if (auto* prm = proc.apvts.getParameter(id)) {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost(prm->convertTo0to1(v));
        prm->endChangeGesture();
    }
}

// Every time the chords change (or you press New melody) we look for a melody that was never used before.
void FireProgressEditor::refreshSeed() {
    const int root = geti("root"), vibe = geti("vibe");
    const bool minor = geti("mode") == 1;
    const auto prog = fire::resolveProgression(geti("source"), vibe, minor, geti("prog"), geti("gen"));
    setParam("seed", (float) proc.freshSeed(prog, root, minor, fire::vibes()[(size_t) vibe].density));
}

void FireProgressEditor::selectProgression(int source, int prog, int gen) {
    setParam("source", (float) source); setParam("prog", (float) prog); setParam("gen", (float) gen);
    refreshSeed();
}

// Builds a brand-new progression from the main chord. Skips anything already in the popular list or history.
void FireProgressEditor::generate() {
    const int vibe = geti("vibe");
    const bool minor = geti("mode") == 1;
    int s = 0;
    for (int t = 0; t < 40; ++t) {
        s = Random::getSystemRandom().nextInt(10000);
        const auto tok = fire::generatedTokens(vibe, minor, (unsigned) s);
        bool dup = false;
        for (int h : genHist) if (fire::generatedTokens(vibe, minor, (unsigned) h) == tok) dup = true;
        for (auto& l : fire::progList(vibe, minor)) if (l == tok) dup = true;
        if (!dup) break;
    }
    genHist.insert(genHist.begin(), s);
    if (genHist.size() > 3) genHist.pop_back();
    selectProgression(1, 0, s);
}

void FireProgressEditor::timerCallback() {
    const int vibe = geti("vibe"), src = geti("source"), prog = geti("prog"), gen = geti("gen"), root = geti("root");
    const bool minor = geti("mode") == 1;
    for (int i = 0; i < 7; ++i) vibeBtn[i].setToggleState(i == vibe, dontSendNotification);

    const auto& list = fire::progList(vibe, minor);
    for (int i = 0; i < 3; ++i) {
        const bool vis = i < (int) list.size();
        popBtn[i].setVisible(vis);
        if (vis) {
            popBtn[i].setButtonText(String(list[(size_t) i]) + "\n" + String(fire::progNames(root, list[(size_t) i])));
            popBtn[i].setToggleState(src == 0 && i == prog % (int) list.size(), dontSendNotification);
        }
        const bool gv = i < (int) genHist.size();
        genBtn[i].setVisible(gv);
        if (gv) {
            const auto tok = fire::generatedTokens(vibe, minor, (unsigned) genHist[(size_t) i]);
            genBtn[i].setButtonText(String(tok) + "\n" + String(fire::progNames(root, tok)));
            genBtn[i].setToggleState(src == 1 && genHist[(size_t) i] == gen, dontSendNotification);
        }
    }

    const auto pr = fire::resolveProgression(src, vibe, minor, prog, gen);
    proc.trackMelody(fire::melodyHash(fire::makeMelody(pr, root, minor, fire::vibes()[(size_t) vibe].density, (unsigned) geti("seed"))));

    const bool synced = proc.hostSynced.load();
    playBtn.setEnabled(!synced);
    bpm.setEnabled(!synced);
    repaint();
}

void FireProgressEditor::resized() {
    auto r = getLocalBounds().reduced(24);
    r.removeFromTop(64);
    auto footerRow = r.removeFromBottom(36);
    r.removeFromBottom(10);
    footer = footerRow;
    auto left = r.removeFromLeft((r.getWidth() - 16) / 2);
    r.removeFromLeft(16);
    auto right = r;

    // ---- left column: main chord + vibe, progressions
    cardMain = left.removeFromTop(120);
    {
        auto c = cardMain.reduced(14, 10); c.removeFromTop(22);
        auto row = c.removeFromTop(30);
        rootBox.setBounds(row.removeFromLeft(90)); row.removeFromLeft(8);
        modeBox.setBounds(row.removeFromLeft(110));
        c.removeFromTop(8);
        auto vr = c.removeFromTop(34);
        const int w = vr.getWidth() / 7;
        for (int i = 0; i < 7; ++i) vibeBtn[i].setBounds(vr.removeFromLeft(w).reduced(2, 0));
    }
    left.removeFromTop(10);
    cardProg = left;
    {
        auto c = cardProg.reduced(14, 10);
        auto head = c.removeFromTop(26);
        auto colL = c.removeFromLeft((c.getWidth() - 12) / 2);
        c.removeFromLeft(12);
        auto colR = c;
        generateBtn.setBounds(head.removeFromRight(110).withSizeKeepingCentre(110, 24));
        for (int i = 0; i < 3; ++i) {
            popBtn[i].setBounds(colL.removeFromTop(46)); colL.removeFromTop(6);
            genBtn[i].setBounds(colR.removeFromTop(46)); colR.removeFromTop(6);
        }
    }

    // ---- right column: chord cards, melody
    chordArea = right.removeFromTop(84);
    right.removeFromTop(10);
    cardMel = right;
    {
        auto c = cardMel.reduced(14, 10); c.removeFromTop(22);
        auto rowB = c.removeFromBottom(34);
        padChords.setBounds(rowB.removeFromLeft((rowB.getWidth() - 8) / 2)); rowB.removeFromLeft(8);
        padMelody.setBounds(rowB);
        c.removeFromBottom(6);
        auto rowA = c.removeFromBottom(30);
        newMelody.setBounds(rowA.removeFromLeft(150)); rowA.removeFromLeft(8);
        chordsBtn.setBounds(rowA.removeFromLeft(90)); rowA.removeFromLeft(8);
        melodyBtn.setBounds(rowA.removeFromLeft(90));
        c.removeFromBottom(8);
        rollArea = c;
    }

    // ---- footer
    auto f = footer;
    playBtn.setBounds(f.removeFromLeft(90).withSizeKeepingCentre(90, 30)); f.removeFromLeft(10);
    bpm.setBounds(f.removeFromLeft(300).withSizeKeepingCentre(300, 26));
}

void FireProgressEditor::paint(Graphics& g) {
    g.setGradientFill(ColourGradient(kBg2, 0.f, 0.f, kBg1, 0.f, (float) getHeight(), false));
    g.fillAll();

    const int root = geti("root"), vibe = geti("vibe"), src = geti("source");
    const bool minor = geti("mode") == 1;
    const auto prog = fire::resolveProgression(src, vibe, minor, geti("prog"), geti("gen"));
    const auto mel = fire::makeMelody(prog, root, minor, fire::vibes()[(size_t) vibe].density, (unsigned) geti("seed"));
    const int cur = proc.currentChord.load();

    // header: title + DAW sync pill
    g.setGradientFill(ColourGradient(kFire, 24.f, 18.f, kGold, 250.f, 18.f, false));
    g.setFont(Font(34.f, Font::bold));
    g.drawText("Fire Progress", 24, 14, 330, 44, Justification::centredLeft);
    {
        const bool synced = proc.hostSynced.load();
        const String t = synced ? "DAW sync  " + String(proc.hostBpm.load(), 1) + " BPM"
                                : "Standalone clock  " + String(geti("bpm")) + " BPM";
        Rectangle<float> pill((float) getWidth() - 24.f - 250.f, 22.f, 250.f, 30.f);
        g.setColour(kCard2); g.fillRoundedRectangle(pill, 15.f);
        g.setColour(synced ? kGold : kMut);
        g.fillEllipse(pill.getX() + 12.f, pill.getCentreY() - 4.f, 8.f, 8.f);
        g.setColour(kText); g.setFont(Font(13.f, Font::bold));
        g.drawText(t, pill.withTrimmedLeft(28.f), Justification::centredLeft);
    }

    // cards
    auto card = [&](Rectangle<int> r, const String& label) {
        g.setColour(kCard); g.fillRoundedRectangle(r.toFloat(), 14.f);
        g.setColour(kLine); g.drawRoundedRectangle(r.toFloat().reduced(0.5f), 14.f, 1.f);
        g.setColour(kMut); g.setFont(Font(11.f, Font::bold));
        g.drawText(label, r.getX() + 14, r.getY() + 8, 300, 16, Justification::centredLeft);
    };
    card(cardMain, "MAIN CHORD  &  VIBE");
    card(cardProg, "POPULAR");
    card(cardMel, "MELODY");
    {
        auto c = cardProg.reduced(14, 10);
        c.removeFromLeft((c.getWidth() - 12) / 2 + 12);
        g.setColour(kMut); g.setFont(Font(11.f, Font::bold));
        g.drawText("GENERATED", c.getX(), cardProg.getY() + 8, 200, 16, Justification::centredLeft);
        if (genHist.empty()) {
            g.setFont(12.f);
            g.drawFittedText("Press Generate to build new progressions that start on your main chord.",
                             c.withTrimmedTop(30), Justification::topLeft, 3);
        }
    }

    // chord cards
    const float cw = (float) chordArea.getWidth() / (float) prog.size();
    for (size_t i = 0; i < prog.size(); ++i) {
        auto r = Rectangle<float>((float) chordArea.getX() + (float) i * cw + 4.f, (float) chordArea.getY(), cw - 8.f, (float) chordArea.getHeight());
        const bool lit = (int) i == cur;
        if (lit) g.setGradientFill(ColourGradient(kFire, r.getX(), r.getY(), kGold, r.getRight(), r.getBottom(), false));
        else     g.setColour(kCard);
        g.fillRoundedRectangle(r, 12.f);
        if (!lit) { g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f); }
        g.setColour(lit ? kBg1 : kText);
        g.setFont(Font(26.f, Font::bold));
        g.drawText(String(fire::chordName(root, prog[i])), r.withTrimmedBottom(26.f), Justification::centred);
        g.setColour(lit ? kBg1 : kMut);
        g.setFont(12.f);
        g.drawText(String(fire::chordNotes(root, prog[i])), r.removeFromBottom(30.f), Justification::centred);
    }

    // melody status badge: new vs used before
    {
        const bool rep = proc.melodyIsRepeat();
        const String t = (rep ? "USED BEFORE" : "NEW MELODY") + String("  -  ") + String(proc.usedCount()) + " heard";
        Rectangle<float> b((float) cardMel.getRight() - 14.f - 190.f, (float) cardMel.getY() + 6.f, 190.f, 20.f);
        if (rep) { g.setColour(kGold); g.drawRoundedRectangle(b, 10.f, 1.5f); g.setColour(kGold); }
        else     { g.setGradientFill(ColourGradient(kFire, b.getX(), b.getY(), kGold, b.getRight(), b.getBottom(), false)); g.fillRoundedRectangle(b, 10.f); g.setColour(kBg1); }
        g.setFont(Font(11.f, Font::bold));
        g.drawText(t, b, Justification::centred);
    }

    // piano roll
    {
        auto rr = rollArea.toFloat();
        g.setColour(kBg1); g.fillRoundedRectangle(rr, 8.f);
        const int T = (int) prog.size() * 8;
        const float sw = rr.getWidth() / (float) T, nh = rr.getHeight() / 26.f;
        g.setColour(kLine);
        for (size_t b = 1; b < prog.size(); ++b) g.fillRect(rr.getX() + (float) b * 8.f * sw, rr.getY(), 1.f, rr.getHeight());
        if (cur >= 0) { g.setColour(kFire.withAlpha(0.2f)); g.fillRect(rr.getX() + (float) cur * 8.f * sw, rr.getY(), 8.f * sw, rr.getHeight()); }
        g.setColour(kGold);
        for (auto& n : mel)
            g.fillRoundedRectangle(rr.getX() + (float) n.step * sw + 1.f, rr.getBottom() - (float) (n.midi - 58) * nh,
                                   jmax((float) n.len * sw - 2.f, 4.f), nh - 1.f, 2.f);
    }

    // footer text
    g.setColour(kMut); g.setFont(12.f);
    const String foot = statusMsg.isNotEmpty() ? statusMsg
        : (proc.hostSynced.load() ? String("Following the DAW tempo and transport.")
                                  : String("Standalone: press Play. In a DAW it follows the host tempo automatically."));
    g.drawFittedText(foot, footer.withTrimmedLeft(420), Justification::centredRight, 2);
}
