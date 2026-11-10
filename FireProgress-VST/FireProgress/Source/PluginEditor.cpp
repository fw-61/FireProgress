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
    if (!b.isEnabled()) { g.setColour(Colours::black.withAlpha(0.5f)); g.fillRoundedRectangle(r, 10.f); }
}

void FireLNF::drawButtonText(Graphics& g, TextButton& b, bool, bool) {
    g.setColour(b.getToggleState() ? kBg1 : kText);
    g.setFont(Font(12.0f, Font::bold));
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
    setSize(1100, 600);
    setWantsKeyboardFocus(true);

    int id = 1;
    for (auto n : fire::NOTE) rootBox.addItem(n, id++);
    modeBox.addItem("Major", 1); modeBox.addItem("Minor", 2);
    barsBox.addItem("4 bars", 1); barsBox.addItem("8 bars", 2);
    for (int i = 1; i <= 8; ++i) countBox.addItem(String(i) + (i == 1 ? " chord" : " chords"), i);
    for (auto* c : { &rootBox, &modeBox, &barsBox, &countBox }) addAndMakeVisible(c);

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

    // sounds: 1 Piano, 2 Synth, 3 Pad (for chords and for melody)
    const char* tones[3] = { "Piano", "Synth", "Pad" };
    for (int i = 0; i < 3; ++i) {
        cTone[i].setButtonText(tones[i]); mTone[i].setButtonText(tones[i]);
        cTone[i].onClick = [this, i] { setParam("csound", (float) i); };
        mTone[i].onClick = [this, i] { setParam("msound", (float) i); };
        addAndMakeVisible(cTone[i]); addAndMakeVisible(mTone[i]);
    }

    // history
    for (int i = 0; i < 4; ++i) {
        histBtn[i].onClick = [this, i] { jumpTo(histIdx[i]); };
        addChildComponent(histBtn[i]);
    }
    undoBtn.onClick = [this] { undo(); };
    redoBtn.onClick = [this] { redo(); };
    listBtn.onClick = [this] { showHistoryList(); };

    for (auto* b : { &generateBtn, &chordsBtn, &melodyBtn, &newMelody, &playBtn, &undoBtn, &redoBtn, &listBtn }) addAndMakeVisible(b);

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
    humanSlider.setSliderStyle(Slider::LinearHorizontal);
    humanSlider.setTextBoxStyle(Slider::TextBoxRight, false, 60, 22);
    humanSlider.setTextValueSuffix(" %");
    addAndMakeVisible(humanSlider);

    rootA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "root", rootBox);
    modeA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "mode", modeBox);
    barsA   = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "bars", barsBox);
    countA  = std::make_unique<APVTS::ComboBoxAttachment>(proc.apvts, "count", countBox);
    chordsA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "chords", chordsBtn);
    melodyA = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "melody", melodyBtn);
    playA   = std::make_unique<APVTS::ButtonAttachment>(proc.apvts, "run", playBtn);
    bpmA    = std::make_unique<APVTS::SliderAttachment>(proc.apvts, "bpm", bpm);
    humanA  = std::make_unique<APVTS::SliderAttachment>(proc.apvts, "human", humanSlider);

    if (geti("source") == 1) genHist.push_back(geti("gen"));
    proc.recordSnapshot(snapshot());
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

// ---------------------------------------------------------------- progressions + melody
// Every time the chords change (or you press New melody) we look for a melody that was never used before.
void FireProgressEditor::refreshSeed() {
    const int root = geti("root"), vibe = geti("vibe"), bars = barsOf();
    const bool minor = geti("mode") == 1;
    const auto segs = fire::buildSegs(geti("source"), vibe, minor, geti("prog"), geti("gen"), countOf(), bars);
    setParam("seed", (float) proc.freshSeed(segs, bars, root, minor, fire::vibes()[(size_t) vibe].density));
}

void FireProgressEditor::selectProgression(int source, int prog, int gen) {
    setParam("source", (float) source); setParam("prog", (float) prog); setParam("gen", (float) gen);
    refreshSeed();
}

// Builds a brand-new progression from the main chord. Skips anything already in the popular list or history.
void FireProgressEditor::generate() {
    const int vibe = geti("vibe"), count = countOf();
    const bool minor = geti("mode") == 1;
    int s = 0;
    for (int t = 0; t < 40; ++t) {
        s = Random::getSystemRandom().nextInt(10000);
        const auto tok = fire::generatedTokens(vibe, minor, (unsigned) s, count);
        bool dup = false;
        for (int h : genHist) if (fire::generatedTokens(vibe, minor, (unsigned) h, count) == tok) dup = true;
        for (int i = 0; i < 3; ++i) if (fire::popularTokens(vibe, minor, i, count) == tok) dup = true;
        if (!dup) break;
    }
    genHist.insert(genHist.begin(), s);
    if (genHist.size() > 3) genHist.pop_back();
    selectProgression(1, 0, s);
}

// ---------------------------------------------------------------- history + undo
FireProgressEditor::Snap FireProgressEditor::snapshot() const {
    return {{ geti("root"), geti("mode"), geti("vibe"), geti("source"), geti("prog"),
              geti("gen"), geti("seed"), geti("count"), geti("bars") }};
}

void FireProgressEditor::applySnapshot(const Snap& s) {
    static const char* ids[9] = { "root", "mode", "vibe", "source", "prog", "gen", "seed", "count", "bars" };
    for (int i = 0; i < 9; ++i) setParam(ids[i], (float) s[(size_t) i]);
    if (s[3] == 1 && std::find(genHist.begin(), genHist.end(), s[5]) == genHist.end()) {
        genHist.insert(genHist.begin(), s[5]);
        if (genHist.size() > 3) genHist.pop_back();
    }
}

void FireProgressEditor::undo() { Snap s; if (proc.historyStep(-1, s)) applySnapshot(s); }
void FireProgressEditor::redo() { Snap s; if (proc.historyStep(+1, s)) applySnapshot(s); }
void FireProgressEditor::jumpTo(int index) { Snap s; if (proc.historyJump(index, s)) applySnapshot(s); }

bool FireProgressEditor::keyPressed(const KeyPress& k) {
    const auto m = k.getModifiers();
    if (m.isCommandDown()) {
        const int c = k.getKeyCode();
        if (c == 'Z' || c == 'z') { if (m.isShiftDown()) redo(); else undo(); return true; }
        if (c == 'Y' || c == 'y') { redo(); return true; }
    }
    return false;
}

String FireProgressEditor::describe(const Snap& s) const {
    const int root = s[0], vibe = s[2];
    const auto chords = fire::resolveChords(s[3], vibe, s[1] == 1, s[4], s[5], s[7] + 1);
    std::string names; int n = 0;
    for (auto& c : chords) {
        if (n++ == 5) { names += " .."; break; }
        names += (names.empty() ? "" : " ") + fire::chordName(root, c);
    }
    return String(names) + "\n" + String(fire::vibes()[(size_t) vibe].name) + " - " + String(s[8] == 1 ? 8 : 4) + " bars";
}

String FireProgressEditor::describeLine(const Snap& s) const {
    const String key = String(fire::NOTE[s[0]]) + (s[1] == 1 ? " minor" : " major");
    return key + "  |  " + describe(s).replace("\n", "  |  ");
}

void FireProgressEditor::showHistoryList() {
    PopupMenu menu;
    const int n = proc.historySize(), pos = proc.historyPos();
    if (n == 0) menu.addItem(1000, "No history yet", false, false);
    for (int i = n - 1; i >= 0 && i >= n - 40; --i)
        menu.addItem(i + 1, describeLine(proc.historyAt(i)), true, i == pos);
    menu.showMenuAsync(PopupMenu::Options().withTargetComponent(&listBtn),
                       [this](int r) { if (r > 0 && r < 1000) jumpTo(r - 1); });
}

// ---------------------------------------------------------------- timer
void FireProgressEditor::timerCallback() {
    const int vibe = geti("vibe"), src = geti("source"), prog = geti("prog"), gen = geti("gen"), root = geti("root");
    const int bars = barsOf(), count = countOf();
    const bool minor = geti("mode") == 1;

    proc.recordSnapshot(snapshot());

    for (int i = 0; i < 7; ++i) vibeBtn[i].setToggleState(i == vibe, dontSendNotification);
    const int plen = (int) fire::progList(vibe, minor).size();
    for (int i = 0; i < 3; ++i) {
        const bool vis = i < plen;
        popBtn[i].setVisible(vis);
        if (vis) {
            const auto tok = fire::popularTokens(vibe, minor, i, count);
            popBtn[i].setButtonText(String(tok) + "\n" + String(fire::progNames(root, tok)));
            popBtn[i].setToggleState(src == 0 && i == prog % plen, dontSendNotification);
        }
        const bool gv = i < (int) genHist.size();
        genBtn[i].setVisible(gv);
        if (gv) {
            const auto tok = fire::generatedTokens(vibe, minor, (unsigned) genHist[(size_t) i], count);
            genBtn[i].setButtonText(String(tok) + "\n" + String(fire::progNames(root, tok)));
            genBtn[i].setToggleState(src == 1 && genHist[(size_t) i] == gen, dontSendNotification);
        }
    }
    for (int i = 0; i < 3; ++i) {
        cTone[i].setToggleState(i == geti("csound"), dontSendNotification);
        mTone[i].setToggleState(i == geti("msound"), dontSendNotification);
    }

    const int hs = proc.historySize(), hp = proc.historyPos();
    for (int i = 0; i < 4; ++i) {
        const int idx = hs - 1 - i;
        histIdx[i] = idx;
        histBtn[i].setVisible(idx >= 0);
        if (idx >= 0) {
            histBtn[i].setButtonText(describe(proc.historyAt(idx)));
            histBtn[i].setToggleState(idx == hp, dontSendNotification);
        }
    }
    undoBtn.setEnabled(hp > 0);
    redoBtn.setEnabled(hp < hs - 1);

    const auto segs = fire::buildSegs(src, vibe, minor, prog, gen, count, bars);
    proc.trackMelody(fire::melodyHash(fire::makeMelody(segs, bars, root, minor, fire::vibes()[(size_t) vibe].density, (unsigned) geti("seed"))));

    const bool synced = proc.hostSynced.load();
    playBtn.setEnabled(!synced);
    bpm.setEnabled(!synced);
    repaint();
}

// ---------------------------------------------------------------- layout
void FireProgressEditor::resized() {
    auto r = getLocalBounds().reduced(24);
    r.removeFromTop(64);
    footer = r.removeFromBottom(36);
    r.removeFromBottom(10);
    auto left = r.removeFromLeft((r.getWidth() - 16) / 2);
    r.removeFromLeft(16);
    auto right = r;

    // ---- left column: main chord, progressions, history
    cardMain = left.removeFromTop(120);
    {
        auto c = cardMain.reduced(14, 10); c.removeFromTop(22);
        auto row = c.removeFromTop(30);
        rootBox.setBounds(row.removeFromLeft(86));  row.removeFromLeft(8);
        modeBox.setBounds(row.removeFromLeft(100)); row.removeFromLeft(8);
        barsBox.setBounds(row.removeFromLeft(96));  row.removeFromLeft(8);
        countBox.setBounds(row.removeFromLeft(110));
        c.removeFromTop(8);
        auto vr = c.removeFromTop(34);
        const int w = vr.getWidth() / 7;
        for (int i = 0; i < 7; ++i) vibeBtn[i].setBounds(vr.removeFromLeft(w).reduced(2, 0));
    }
    left.removeFromTop(10);
    cardProg = left.removeFromTop(204);
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
    left.removeFromTop(10);
    cardHist = left;
    {
        auto c = cardHist.reduced(14, 8);
        auto head = c.removeFromTop(24);
        listBtn.setBounds(head.removeFromRight(56)); head.removeFromRight(6);
        redoBtn.setBounds(head.removeFromRight(56)); head.removeFromRight(6);
        undoBtn.setBounds(head.removeFromRight(56));
        c.removeFromTop(4);
        const int w = c.getWidth() / 4;
        for (int i = 0; i < 4; ++i) histBtn[i].setBounds(c.removeFromLeft(w).reduced(2, 0));
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
        c.removeFromBottom(6);
        auto rowMT = c.removeFromBottom(28);
        c.removeFromBottom(4);
        auto rowCT = c.removeFromBottom(28);
        c.removeFromBottom(6);
        auto rowH = c.removeFromBottom(26);
        c.removeFromBottom(8);
        rollArea = c;
        lblMT = rowMT.removeFromLeft(110);
        for (int i = 0; i < 3; ++i) { mTone[i].setBounds(rowMT.removeFromLeft(84)); rowMT.removeFromLeft(6); }
        lblCT = rowCT.removeFromLeft(110);
        for (int i = 0; i < 3; ++i) { cTone[i].setBounds(rowCT.removeFromLeft(84)); rowCT.removeFromLeft(6); }
        lblH = rowH.removeFromLeft(110);
        humanSlider.setBounds(rowH.removeFromLeft(300));
    }

    // ---- footer
    auto f = footer;
    playBtn.setBounds(f.removeFromLeft(90).withSizeKeepingCentre(90, 30)); f.removeFromLeft(10);
    bpm.setBounds(f.removeFromLeft(300).withSizeKeepingCentre(300, 26));
}

// ---------------------------------------------------------------- painting
void FireProgressEditor::paint(Graphics& g) {
    g.setGradientFill(ColourGradient(kBg2, 0.f, 0.f, kBg1, 0.f, (float) getHeight(), false));
    g.fillAll();

    const int root = geti("root"), vibe = geti("vibe"), src = geti("source"), bars = barsOf(), count = countOf();
    const bool minor = geti("mode") == 1;
    const auto segs = fire::buildSegs(src, vibe, minor, geti("prog"), geti("gen"), count, bars);
    const auto mel = fire::makeMelody(segs, bars, root, minor, fire::vibes()[(size_t) vibe].density, (unsigned) geti("seed"));
    const double L = bars * 4.0, beat = proc.currentBeat.load();
    int cur = -1;
    if (beat >= 0.0) for (size_t i = 0; i < segs.size(); ++i) if (beat >= segs[i].start) cur = (int) i;

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
    card(cardMain, "MAIN CHORD,  PATTERN SIZE  &  VIBE");
    card(cardProg, "POPULAR");
    card(cardMel, "MELODY");
    card(cardHist, "HISTORY   (Ctrl+Z undo, Ctrl+Shift+Z redo)");
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
    g.setColour(kMut); g.setFont(Font(11.f, Font::bold));
    g.drawText("CHORD SOUND", lblCT, Justification::centredLeft);
    g.drawText("MELODY SOUND", lblMT, Justification::centredLeft);
    g.drawText("HUMANIZE", lblH, Justification::centredLeft);

    // chord cards: width follows how long each chord lasts
    for (size_t i = 0; i < segs.size(); ++i) {
        const float x0 = (float) chordArea.getX() + (float) (segs[i].start / L) * (float) chordArea.getWidth();
        const float x1 = (float) chordArea.getX() + (float) ((segs[i].start + segs[i].len) / L) * (float) chordArea.getWidth();
        auto r = Rectangle<float>(x0 + 2.f, (float) chordArea.getY(), x1 - x0 - 4.f, (float) chordArea.getHeight());
        const bool lit = (int) i == cur;
        if (lit) g.setGradientFill(ColourGradient(kFire, r.getX(), r.getY(), kGold, r.getRight(), r.getBottom(), false));
        else     g.setColour(kCard);
        g.fillRoundedRectangle(r, 12.f);
        if (!lit) { g.setColour(kLine); g.drawRoundedRectangle(r.reduced(0.5f), 12.f, 1.f); }
        g.setColour(lit ? kBg1 : kText);
        g.setFont(Font(count <= 4 ? 26.f : (count <= 6 ? 21.f : 16.f), Font::bold));
        g.drawFittedText(String(fire::chordName(root, segs[i].chord)), r.withTrimmedBottom(26.f).toNearestInt(), Justification::centred, 1);
        g.setColour(lit ? kBg1 : kMut);
        g.setFont(11.f);
        g.drawFittedText(String(fire::chordNotes(root, segs[i].chord)), r.removeFromBottom(30.f).toNearestInt(), Justification::centred, 1);
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

    // piano roll with chord names and a moving playhead
    {
        auto rr = rollArea.toFloat();
        g.setColour(kBg1); g.fillRoundedRectangle(rr, 8.f);
        const int steps = bars * 8;
        const float sw = rr.getWidth() / (float) steps, nh = rr.getHeight() / 26.f;
        g.setColour(kLine);
        for (int b = 1; b < bars; ++b) g.fillRect(rr.getX() + (float) b * 8.f * sw, rr.getY(), 1.f, rr.getHeight());
        if (cur >= 0) {
            g.setColour(kFire.withAlpha(0.2f));
            g.fillRect(rr.getX() + (float) (segs[(size_t) cur].start / L) * rr.getWidth(), rr.getY(),
                       (float) (segs[(size_t) cur].len / L) * rr.getWidth(), rr.getHeight());
        }
        g.setFont(Font(10.f, Font::bold));
        for (size_t i = 0; i < segs.size(); ++i) {
            const float x = rr.getX() + (float) (segs[i].start / L) * rr.getWidth();
            if (segs[i].start > 0) { g.setColour(kMut.withAlpha(0.55f)); g.fillRect(x, rr.getY(), 1.5f, rr.getHeight()); }
            g.setColour(kMut);
            g.drawText(String(fire::chordName(root, segs[i].chord)), (int) x + 3, (int) rr.getY() + 1, 60, 12, Justification::centredLeft);
        }
        g.setColour(kGold);
        for (auto& n : mel)
            g.fillRoundedRectangle(rr.getX() + (float) n.step * sw + 1.f, rr.getBottom() - (float) (n.midi - 58) * nh,
                                   jmax((float) n.len * sw - 2.f, 4.f), nh - 1.f, 2.f);
        if (beat >= 0.0) {
            g.setColour(kText);
            g.fillRect(rr.getX() + (float) (beat / L) * rr.getWidth() - 1.f, rr.getY(), 2.f, rr.getHeight());
        }
    }

    // footer text
    g.setColour(kMut); g.setFont(12.f);
    const String foot = statusMsg.isNotEmpty() ? statusMsg
        : (proc.hostSynced.load() ? String("Following the DAW tempo and transport.")
                                  : String("Standalone: press Play. In a DAW it follows the host tempo automatically."));
    g.drawFittedText(foot, footer.withTrimmedLeft(420), Justification::centredRight, 2);
}
